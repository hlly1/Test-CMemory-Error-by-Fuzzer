#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include "debug.h"

#if defined(DC_LIBFUZZER) || defined(DC_FUZZ)
#include <stdint.h>
#define LIBFUZZER_INPUT_FILE "libFuzzerInput.XXXXXX";
/* turn off tracing to make it run faster */
#define printf(...)
#define fprintf(...)
#endif

const char INSTRUCTION_ADD[] = "+";

const char INSTRUCTION_MULT[] = "*";

const char INSTRUCTION_SUB[] = "-";

const char INSTRUCTION_DIV[] = "/";

const char INSTRUCTION_PUSH[] = "push";

const char INSTRUCTION_POP[] = "pop";

const char INSTRUCTION_LOAD[] = "load";

const char INSTRUCTION_REM[] = "remove";

const char INSTRUCTION_STORE[] = "store";

const char INSTRUCTION_SAVE[] = "save";

const char INSTRUCTION_LIST[] = "list";

const char INSTRUCTION_PRINT[] = "print";

/* variables are signed 32-bit integers */
typedef int32_t value_t;

/* we store a mapping from variable names to values using a binary tree
   to try to ensure log lookup performance */
typedef struct node {
  char * varname;
  value_t value;
  struct node *left;
  struct node *right;
} node_t;

static const node_t * lookup(const node_t *p, const char *varname){
  while (p != NULL){
    int ret = strcmp(varname,p->varname);
    if (ret == 0){
      return p;
    }else if (ret < 0){
      p = p->left;
    }else{
      p = p->right;
    }
  }
  return p; // not found
}

static void node_print(const node_t *p){
  printf("VARIABLE: %s, VALUE: %d\n",p->varname,p->value);
}

/* construct a new node */
static node_t *node_new(const char *varname, const value_t value){
  node_t *new = malloc(sizeof(node_t));
  assert(new != NULL && "new: malloc failed");
  new->varname = strdup(varname);
  assert(new->varname != NULL && "new: strdup varname failed");
  new->value = value;
  new->left = NULL;
  new->right = NULL;
  return new;
}

/* updates a node's value in place: 
   replaces p's value with that from q and frees q */
static void node_edit_value(node_t * p, node_t *q){  
  p->value = q->value;
  free(q->varname);
  free(q);
}

static void node_free(node_t *p){
  free(p->varname);
  free(p);
}

/* insert q into p
   we assume that if q has children then it cannot already
   be present in p. Otherwise, if q has no children and we find its varname in p,
   then we edit the found entry in place while preserving its children */
static node_t * node_insert(node_t *p, node_t *q){
  if (p == NULL){
    return q;
  }
  if (q == NULL){
    return p;
  }
  /* we store a pointer to a node pointer that remembers where in the 
     tree the new node needs to be added */
  node_t ** new = NULL;
  node_t * const start = p;
  while (new == NULL) {
    int ret = strcmp(q->varname,p->varname);
    if (ret == 0){
      assert (q->left == NULL && q->right == NULL && "illegal insertion");
      /* edit the node in place */
      node_edit_value(p,q);
      /* q is now freed so cannot be used anymore */
      return start;
    }else if (ret < 0){
      if (p->left == NULL){
        new = &(p->left);
      }else{
        p = p->left;
      }
    }else{
      if (p->right == NULL){
        new = &(p->right);
      }else{
        p = p->right;
      }
    }
  }
  *new = q;
  return start;
}

/* returns a pointer to the tree with the node added or with the existing
   node updated if it was  already present */
static node_t * put(node_t *p, const char *varname, const value_t value){
  return node_insert(p,node_new(varname,value));
}

/* destroy tree rooted at p */
static void destroy(node_t *p){
  while (p != NULL){
    node_t * left = p->left;
    node_t * const right = p->right;
    left = node_insert(left,right);
    node_free(p);
    p = left;
  }
}

/* returns a pointer to the tree with the node removed (if it was present) */
static node_t * rem(node_t *p, const char *varname){
  node_t * const start = p;
  /* remember where the pointer to p was stored */
  node_t ** pptr = NULL;
  while (p != NULL){
    int ret = strcmp(varname,p->varname);
    if (ret == 0){
      node_t * left = p->left;
      node_t * const right = p->right;
      left = node_insert(left,right);
      node_free(p);
      if (pptr != NULL){
        *pptr = left;
        return start;
      }else{
        /* p was the only node in the tree */
        assert(p == start);
        return left;
      }
    }else if (ret < 0){
      pptr = &(p->left);
      p = p->left;
    }else{
      pptr = &(p->right);
      p = p->right;
    }
  }
  return start; // not found
}

const char WHITESPACE[] = " \t\r\n";


/* tokenise a string, splitting on characters in WHITESPACE, up to
 * a maxium of toksLen tokens, each of whose start addresses is put into
 * toks and each of which is NUL-terminated in str.
 * returns number of tokens found */
unsigned int tokenise(char *str, char * toks[], unsigned int toksLen){
  unsigned numToks = 0;
  while (numToks < toksLen){
    /* strip leading whitespace */     
    size_t start = strspn(str,WHITESPACE);
    if (str[start] != '\0'){
      toks[numToks] = &(str[start]);    
    
      /* compute the length of the token */
      const size_t tokLen = strcspn(toks[numToks],WHITESPACE);
      if (tokLen > 0){
        toks[numToks][tokLen] = '\0';
        str = &(toks[numToks][tokLen+1]);
        numToks++;
      }else{
        return numToks;
      }
    }else{
      return numToks;
    }
  }
  return numToks;
}

#define MAX_LINE_LENGTH  1022
#define MAX_INSTRUCTIONS 1024
/* two extra chars in each line: the newline '\n' and NUL '\0' */
#define INSTRUCTION_LENGTH (MAX_LINE_LENGTH+2)

/* maximum stack size */
#define STACK_CAPACITY 512


/* a global instruction buffer */
char inst[INSTRUCTION_LENGTH];

/* value mapping for each varname: initially empty */
node_t * map = NULL;

/* the stack */
value_t stack[STACK_CAPACITY];

/* the stack pointer: always points at the next free spot on the stack.
   Invariant: 0 <= sp <= STACK_CAPACITY.
   Stack is empty when sp == 0. Stack is full when sp == STACK_CAPACITY.
   Number of free spots in the stack is always: STACK_CAPACITY - sp */
unsigned int sp = 0;

/* assumes stack has enough room */
static void stack_push(const value_t val){
  stack[sp] = val;
  sp++;
}

static unsigned int stack_size(void){
  return sp;
}

static int stack_empty(){
  return (stack_size() == 0);
}

/* assumes stack is non-empty */
static value_t stack_pop(void){
  sp--;
  return stack[sp];
}

static void stack_print(void){
  unsigned int psp = sp;
  while (psp > 0){
    psp--;
    printf("%d\n",stack[psp]);
  }
}


static int stack_full(void){
  return (sp == STACK_CAPACITY);
}

/* a doubly-linked list of node pointers
   is used to implement stacks/queues of nodes so we can implement various
   tree traversal algorithms without using recursion (to avoid stack overflow
   for very large trees). Stack overflow is a trivial form of memory-safety
   vulnerability. */
typedef struct nodeptr_list_elem {
  const node_t *p;
  struct nodeptr_list_elem *next;
  struct nodeptr_list_elem *prev;
} nodeptr_list_elem_t;

typedef struct nodeptr_list {
  nodeptr_list_elem_t *head;
  nodeptr_list_elem_t *last;
} nodeptr_list_t;


/* push an element p onto the front of a nodeptr list lst */
nodeptr_list_t list_push(nodeptr_list_t lst, const node_t *p){
  nodeptr_list_elem_t *n = malloc(sizeof(nodeptr_list_elem_t));
  assert(n != NULL && "push: malloc failed");
  n->p = p;
  n->next = lst.head;
  n->prev = NULL;  
  if (lst.head != NULL){
    assert(lst.last != NULL);
    lst.head->prev = n;
  }else{
    assert(lst.last == NULL);
    lst.last = n;
  }
  lst.head = n;
  
  return lst;
}

/* when out is non-NULL we place a pointer to the first node into it.
   assumption: lst.head and lst.last are non-NULL */
nodeptr_list_t list_pop(nodeptr_list_t lst, const node_t **out){
  assert(lst.head != NULL && lst.last != NULL);
  if (out != NULL){
    *out = lst.head->p;
  }
  if (lst.last == lst.head){
    free(lst.head);
    lst.head = NULL;
    lst.last = NULL;
  }else{
    nodeptr_list_elem_t *ret = lst.head->next;
    free(lst.head);
    lst.head = ret;
  }
  return lst;
}

/* when out is non-NULL we place a pointer to the last node into it.
   assumption: lst.head and lst.last are non-NULL */
nodeptr_list_t list_dequeue(nodeptr_list_t lst, const node_t **out){
  assert(lst.head != NULL && lst.last != NULL);
  if (out != NULL){
    *out = lst.last->p;
  }

  if (lst.last == lst.head){
    free(lst.head);
    lst.head = NULL;
    lst.last = NULL;
  }else{
    nodeptr_list_elem_t *ret = lst.last->prev;
    free(lst.last);
    lst.last = ret;
  }
  return lst;
}

/* in order traversal to print out nodes in sorted order. Is used to
   implement listing of all entries in the dc */
void print_inorder(const node_t *p){
  nodeptr_list_t lst = {.head = NULL, .last = NULL};
  if (p != NULL){
    lst = list_push(lst,p);

    while(lst.head != NULL){
      // keep recursing left until we can go no further
      while (p->left != NULL){
        lst = list_push(lst,p->left);
        p = p->left;
      }
      
      // pop from the stack to simulate the return
      const node_t *q;
      lst = list_pop(lst,&q);

      // print the node following the return
      node_print(q);

      // simulate right recursive call
      if (q->right != NULL){
        lst = list_push(lst,q->right);
        p = q->right;
      }
    }
  }
}

/* save a node to the given file. 
   We save to the file a "push/store" combination for each node
   that will cause the node to be placed back into the dc when the
   file is read. */
void node_save(const node_t *p, FILE *f){
  fprintf(f,"%s",INSTRUCTION_PUSH);
  fprintf(f," ");
  fprintf(f,"%d",p->value);  
  fprintf(f,"\n");
  
  fprintf(f,"%s",INSTRUCTION_STORE);
  fprintf(f," ");
  fprintf(f,"%s",p->varname);
  fprintf(f,"\n");
}

/* level order (i.e. breadth-first) traversal to print nodes out in the
   order that they need to be put back in to an empty tree to ensure
   that the resulting tree has the same structure as the original one.
   This is how we save the dc to a file.
   Returns 0 on success; nonzero on failure */
int save_levelorder(const node_t *p,
                    const char * filename){
#ifdef DC_FUZZ
  // ignore the file name when fuzzing etc. to avoid DoS on the server
  FILE *f = fopen("/dev/null","w");
#else
  FILE *f = fopen(filename,"w");
#endif
  if (f == NULL){
    fprintf(stderr,"Couldn't open file %s for writing.\n",filename);
    return -1;
  }
  nodeptr_list_t lst = {.head = NULL, .last = NULL};
  if (p != NULL){
    lst = list_push(lst,p);

    while(lst.last != NULL){
      lst = list_dequeue(lst,&p);
      node_save(p,f);
      if (p->left != NULL){
        lst = list_push(lst,p->left);
      }
      if (p->right != NULL){
        lst = list_push(lst,p->right);
      }
    }
  }
  fclose(f);
  return 0;
}

/* returns 0 on successful execution of the instruction in inst */
static int execute(void){
  char * toks[4]; /* these are pointers to start of different tokens */
  const unsigned int numToks = tokenise(inst,toks,4);
    
  if (numToks == 0){
    /* blank line */
    return 0;
  }
    
  if (strcmp(toks[0],INSTRUCTION_LOAD) == 0){
    if (numToks != 2){
      debug_printf("Expected 1 argument to %s instruction but instead found %u\n",INSTRUCTION_LOAD,numToks-1);
      return -1;
    }
    debug_printf("Looking up: %s\n",toks[1]);
    const node_t *p = lookup(map,toks[1]);
    if (p != NULL){
      if (stack_full()){
        debug_printf("Trying to load onto full stack\n");
        return -1;
      }
      stack_push(p->value);      
    }else{
      printf("Not found.\n");
    }
    
  } else if (strcmp(toks[0],INSTRUCTION_STORE) == 0){
    if (numToks != 2){
      debug_printf("Expected 1 argument to %s instruction but instead found %u\n",INSTRUCTION_STORE,numToks-1);
      return -1;
    }
    
    if (stack_empty()){
      debug_printf("Store from empty stack\n");
      return -1;
    }
    value_t value = stack_pop();
    map = put(map,toks[1],value);

  } else if (strcmp(toks[0],INSTRUCTION_ADD) == 0){
    if (numToks != 1){
      debug_printf("Expected 0 arguments to %s instruction but instead found %u\n",INSTRUCTION_ADD,numToks-1);
      return -1;
    }
    
    if (stack_size() < 2){
      debug_printf("Add from insufficient stack\n");
      return -1;
    }
    value_t a = stack_pop();
    value_t b = stack_pop();

    stack_push(a+b);

  } else if (strcmp(toks[0],INSTRUCTION_SUB) == 0){
    if (numToks != 1){
      debug_printf("Expected 0 arguments to %s instruction but instead found %u\n",INSTRUCTION_SUB,numToks-1);
      return -1;
    }
    
    if (stack_size() < 2){
      debug_printf("Sub from insufficient stack\n");
      return -1;
    }
    value_t a = stack_pop();
    value_t b = stack_pop();

    stack_push(a-b);

  } else if (strcmp(toks[0],INSTRUCTION_MULT) == 0){
    if (numToks != 1){
      debug_printf("Expected 0 arguments to %s instruction but instead found %u\n",INSTRUCTION_MULT,numToks-1);
      return -1;
    }
    
    if (stack_size() < 2){
      debug_printf("Mult from insufficient stack\n");
      return -1;
    }
    value_t a = stack_pop();
    value_t b = stack_pop();

    stack_push(a*b);

  } else if (strcmp(toks[0],INSTRUCTION_DIV) == 0){
    if (numToks != 1){
      debug_printf("Expected 0 arguments to %s instruction but instead found %u\n",INSTRUCTION_DIV,numToks-1);
      return -1;
    }
    
    if (stack_size() < 2){
      debug_printf("Add from insufficient stack\n");
      return -1;
    }
    value_t a = stack_pop();
    value_t b = stack_pop();
    if (b == 0){
      debug_printf("Refusing to divide by zero\n");
      return -1;
    }
    stack_push(a/b);
    
  } else if (strcmp(toks[0],INSTRUCTION_POP) == 0){
    if (numToks != 1){
      debug_printf("Expected 0 arguments to %s instruction but instead found %u\n",INSTRUCTION_POP,numToks-1);
      return -1;
    }
    
    if (stack_empty()){
      debug_printf("Pop from empty stack\n");
      return -1;
    }
    value_t value = stack_pop();

  } else if (strcmp(toks[0],INSTRUCTION_PUSH) == 0){
    if (numToks != 2){
      debug_printf("Expected 1 arguments to %s instruction but instead found %u\n",INSTRUCTION_PUSH,numToks-1);
      return -1;
    }
    
    if (stack_full()){
      debug_printf("Push to full stack\n");
      return -1;
    }
    value_t value;
    if (sscanf(toks[1],"%d",&value) != 1){
      debug_printf("Non-numeric argument to %s instruction: %s\n",INSTRUCTION_PUSH,toks[1]);
      return -1;
    }
    stack_push(value);
    
    
  } else if (strcmp(toks[0],INSTRUCTION_REM) == 0){
    if (numToks != 2){
      debug_printf("Expected 1 argument to %s instruction but instead found %u\n",INSTRUCTION_REM,numToks-1);
      return -1;
    }
    debug_printf("Removing: %s\n",toks[1]);
    map = rem(map,toks[1]);
    
  } else if (strcmp(toks[0],INSTRUCTION_SAVE) == 0){
    if (numToks != 2){
      debug_printf("Expected 1 arguments to %s instruction but instead found %u\n",INSTRUCTION_SAVE,numToks-1);
      return -1;
    }
    debug_printf("Saving to file: %s\n",toks[1]);
    if (save_levelorder(map,toks[1]) != 0){
      debug_printf("Error saving to file %s\n",toks[1]);
      return -1;
    }

  } else if (strcmp(toks[0],INSTRUCTION_LIST) == 0){
    if (numToks != 1){
      debug_printf("Expected 0 arguments to %s instruction but instead found %u\n",INSTRUCTION_LIST,numToks-1);
      return -1;
    }
    print_inorder(map);

  } else if (strcmp(toks[0],INSTRUCTION_PRINT) == 0){
    if (numToks != 1){
      debug_printf("Expected 0 arguments to %s instruction but instead found %u\n",INSTRUCTION_PRINT,numToks-1);
      return -1;
    }
    stack_print();
    
  }else{
    debug_printf("Unrecognised instruction %s\n",toks[0]);
    return -1;
  }
  
  return 0;
}

/* returns >=0 on success, in which case the number of instructions executed
   is returned. Returns < 0 on failure. */
static int run(FILE *f){
  assert(f != NULL);
  
  int instructionCount = 0;
  while (instructionCount < MAX_INSTRUCTIONS){
    memset(inst,0,sizeof(inst));
    char * res = fgets(inst,sizeof(inst),f);
    if (res == NULL){
      if (feof(f)){
        /* end of file */
        return instructionCount;
      }else{
        debug_printf("Error while reading, having read %d lines\n",instructionCount);
        return -1;
      }
    }
    if (inst[MAX_LINE_LENGTH] != '\0'){
      if (!(inst[MAX_LINE_LENGTH] == '\n' && inst[MAX_LINE_LENGTH+1] == '\0')){
        fprintf(stderr,"Line %d exceeds maximum length (%d)\n",instructionCount+1,MAX_LINE_LENGTH);
        debug_printf("(Expected at array index %d to find NUL but found '%c' (%d))\n",MAX_LINE_LENGTH,inst[MAX_LINE_LENGTH],inst[MAX_LINE_LENGTH]);
        return -1;
      }
    }else{
      /* inst[MAX_LINE_LENGTH] == '\0', so
         strlen is guaranteed to be <= MAX_LINE_LENGTH
         Check if it has a newline and add it if it needs it */
      size_t len = strlen(inst);
      if (len > 0){
        if (inst[len-1] != '\n'){
          inst[len] = '\n';
          inst[len+1] = '\0';
        }
      }
    }
    instructionCount++;
    int r = execute();
    if (r != 0){
      return -1;
    }
  }

  if (feof(f)){
    /* final line of file didn't have a trailing newline */
    return instructionCount;
  }else{
    /* see if we are at end of file by trying to do one more read.
       this is necessary if the final line of the file ends in a 
       newline '\n' character */
    char c;
    int res = fread(&c,1,1,f);
    if (res == 1){
      fprintf(stderr,"Number of instructions (lines) in file exceeds max (%d)\n",MAX_INSTRUCTIONS);
      return -1;
    }else{
      if (feof(f)){
        /* final read found the EOF, so all good */
        return instructionCount;
      }else{
        /* probably won't ever get here */
        debug_printf("Error while trying to test if line %d was empty\n",instructionCount+1);
        return -1;
      }
    }
  }
}

#ifdef DC_LIBFUZZER
int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  /* create a temporary file, save the libfuzzer input there */
  char libfuzzer_input_file[] = LIBFUZZER_INPUT_FILE;
  int fd = mkstemp(libfuzzer_input_file);
  write(fd,Data,Size);
  close(fd);
  /* now run using that file as input */
  FILE *f = fopen(libfuzzer_input_file,"r");
  run(f);
  fclose(f);
  /* delete the temporary file */
  unlink(libfuzzer_input_file);
  destroy(map);
  map = NULL;
  return 0; /* libFuzzer wants 0 returned always */
}
#else
int main(const int argc, const char * argv[]){
  if (argc <= 1){
    fprintf(stderr,"Usage: %s file1 file2 ...\n",argv[0]);
    fprintf(stderr,"       use - to read from standard input\n");
    exit(0);
  }
  
  for (int i = 1; i<argc; i++){
    printf("Running on input file %s\n",argv[i]);
    FILE *f;
    if (strcmp(argv[i],"-") == 0){
      f = stdin;
    }else{
      f = fopen(argv[i],"r");
      if (f == NULL){
        fprintf(stderr,"Error opening %s for reading\n",argv[i]);
        destroy(map);
        exit(1);
      }
    }
    int ans = run(f);
    if (ans < 0){
      fprintf(stderr,"Error\n");
    }
    /* do not close stdin */
    if (f != stdin){
      fclose(f);
    }
  }
  destroy(map);
  return 0;
}
#endif
