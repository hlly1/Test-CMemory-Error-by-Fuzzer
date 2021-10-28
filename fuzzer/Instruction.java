import java.util.Arrays;
import java.util.ArrayList;

public enum Instruction {
    PLUS("+",new OperandType[]{}),
    SUB("-",new OperandType[]{}),
    MULT("*",new OperandType[]{}),
    DIV("/",new OperandType[]{}),
    PUSH("push",new OperandType[]{OperandType.STRING}),
    POP("pop",new OperandType[]{}),
    LOAD("load",new OperandType[]{OperandType.STRING}),
    REM("remove",new OperandType[]{OperandType.STRING}),
    STORE("store",new OperandType[]{OperandType.STRING}),
    SAVE("save",new OperandType[]{OperandType.STRING}),
    LIST("list",new OperandType[]{}),
    PRINT("print",new OperandType[]{});

    public static String getBNF(){
        String grammar = "<INSTRUCTION> ::= \n";
        Instruction[] INSTS = Instruction.values();
        boolean firstInst = true;
        for (Instruction inst : INSTS){
            if (firstInst){
                grammar += "      \"";
                firstInst = false;
            }else{
                grammar += "    | \"";
            }
            grammar += inst.getOpcode() + "\"";
            for (OperandType op : inst.getOperands()){
                grammar += " <" + op.toString() + ">";
            }
            grammar += "\n";
        }
        return grammar;
    }
    
    private final String opcode;
    private final OperandType[] operands;

    Instruction(String opcode, OperandType[] operands){
        this.opcode = opcode;
        this.operands = operands;
    }

    public String getOpcode(){
        return opcode;
    }
    
    public OperandType[] getOperands(){
        return operands;
    }

    public String toString(){
        String operandsString = "";
        for (OperandType op : operands) {
            operandsString += " " + op.toString();
        }
        return "\"" + opcode + "\"" + operandsString;
    }
    
}
