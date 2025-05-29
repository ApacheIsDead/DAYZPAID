Rudamentary DayZ kernel driver that uses .txt file and sneaky method of manually accessing loader (client) memory  and using a structure stored in the live program to communicate, usermode just does structData.targetAddr = exampleAddr;

the driver is already watching and detects it.
