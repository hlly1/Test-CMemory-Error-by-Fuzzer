import java.io.IOException;
import java.io.FileOutputStream;
import java.io.PrintWriter;
import java.util.Random;

/* a stub for your team's fuzzer */
public class Fuzzer {

	private static Instruction[] INSTRUCTIONS = Instruction.values();
	private static PrintWriter pw  = null;
	private static final int MAX_LINE_LENGTH = 1022;
	private static final int MAX_INSTRUCTIONS = 1024;

	public static void main(String[] args) throws IOException {
		System.out.println(Instruction.getBNF());
		//int instructionCount = getRandomInt(0, MAX_INSTRUCTIONS);
		try{
			boolean hasPoc = false;
			for(int i = 0; i<300;i++){
				generateCorpus(hasPoc);
				hasPoc = true;
			}
			
		}catch(Exception e){
			System.out.println("Error during generating corpus...");
		}
	}

	public static void generateCorpus(boolean hasPoc) throws Exception{
		String OUTPUT_FILE = "..\\tests\\fuzz"+System.currentTimeMillis()+".txt";
		FileOutputStream out = null;
		PrintWriter pw = null;
		try {
			out = new FileOutputStream(OUTPUT_FILE);
			pw = new PrintWriter(out);

			int maxLenth = MAX_INSTRUCTIONS;
			if(!hasPoc){
				//special command combination
				String pocStr135 = "push "+ getRandomInt(0, MAX_LINE_LENGTH - "push ".length())+"\n"+"store "+getRandomName(MAX_LINE_LENGTH - "store ".length(), false);
				String pocStr2 = "push "+ getRandomInt(0, MAX_LINE_LENGTH - "push ".length())+"\n"+"+"+"\n"+"print";
				String pocStr4 = "push "+ getRandomInt(0, MAX_LINE_LENGTH - "push ".length())+"\n"+"-"+"\n"+"print";
				pw.println(pocStr135);
				pw.println(pocStr2);
				pw.println(pocStr4);
				maxLenth = MAX_INSTRUCTIONS-8;
			}
			for (int i = 0; i < maxLenth; i++) {
				Instruction instruction = getRandomInstruction();
				String outputString = instruction.getOpcode();
				if (instruction.equals(Instruction.PUSH) || instruction.equals(Instruction.LOAD)
						|| instruction.equals(Instruction.REM) || instruction.equals(Instruction.STORE)) {
					double spaceType = Math.random();
					if(spaceType > 0.8){
						outputString += "\t";
					}else{
						outputString += " ";
					}
					outputString += getRandomName(getRandomInt(0, MAX_LINE_LENGTH - outputString.length() - 1), false);
				} else if (instruction.equals(Instruction.SAVE)) {
					double spaceType = Math.random();
					if(spaceType > 0.8){
						outputString += "\t";
					}else{
						outputString += " ";
					}
					outputString += getRandomName(getRandomInt(0, MAX_LINE_LENGTH - outputString.length() - 5), true);
					outputString += ".txt";
				}
				pw.println(outputString);
			}

		} catch (Exception e) {
			e.printStackTrace(System.err);
			System.exit(1);
		} finally {
			if (pw != null) {
				pw.flush();
			}
			if (out != null) {
				out.close();
			}
		}
	}

	public static Instruction getRandomInstruction() {
		int numInstruction = Instruction.values().length;
		int index = getRandomInt(0, numInstruction - 1);
		return INSTRUCTIONS[index];
	}

	public static String getRandomName(int maxLenth, boolean isSAVE) {
		StringBuffer stringBuffer = new StringBuffer();
		int stringType = getRandomInt(0, 3);
		switch (stringType) {
		case 0:
		//Mix arguments
			for (int i = 0; i < maxLenth; i++) {
				int charType = getRandomInt(0, 3);
				long asci = 0;
				switch (charType) {
				case 0:
					asci = Math.round(Math.random() * 25 + 65);
					if(Math.random()>0.95)
					{
						asci=32;
					}
					stringBuffer.append(String.valueOf((char) asci));
					break;
				case 1:
					asci = Math.round(Math.random() * 25 + 97);
					stringBuffer.append(String.valueOf((char) asci));
					break;
				case 2:
					stringBuffer.append(String.valueOf(getRandomInt(0, 9)));
					break;
				case 3:
					asci = getRandomInt(32, 126);
					stringBuffer.append(String.valueOf((char) asci));
					break;
				}
			}
			break;
		case 1:
			for (int i = 0; i < maxLenth; i++) {
				long asci = 0;
				asci = Math.round(Math.random() * 25 + 65);
				if(Math.random()>0.95)
				{
					asci=32;
				}
				stringBuffer.append(String.valueOf((char) asci));
			}
			break;
		case 2:
			boolean isFloat = false;
			for (int i = 0; i < maxLenth; i++) {
				if(Math.random()>0.970 && !isFloat && !isSAVE)
				{
					stringBuffer.append(".");
					isFloat = true;
				}
				stringBuffer.append(String.valueOf(getRandomInt(0, 9)));
				
			}
			break;
		case 3:
			for (int i = 0; i < maxLenth; i++) {
				long asci = 0;
				asci = Math.round(Math.random() * 25 + 97);
				stringBuffer.append(String.valueOf((char) asci));
			}
			break;
		default:
			break;
		}

		return stringBuffer.toString();
	}

	public static int getRandomInt(int min, int max) {
		int randomNumber = new Random().nextInt(max + 1 - min) + min;
		return randomNumber;
	}

}
