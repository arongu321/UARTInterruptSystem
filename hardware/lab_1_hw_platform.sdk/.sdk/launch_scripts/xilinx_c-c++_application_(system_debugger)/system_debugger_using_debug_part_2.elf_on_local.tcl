connect -url tcp:127.0.0.1:3121
source C:/Users/ecetech/Desktop/ECE315_labs/lab1/lab_1_new/lab_1_new.sdk/design_1_wrapper_hw_platform_0/ps7_init.tcl
targets -set -nocase -filter {name =~"APU*" && jtag_cable_name =~ "Digilent Zybo Z7 210351AB6E75A"} -index 0
rst -system
after 3000
targets -set -filter {jtag_cable_name =~ "Digilent Zybo Z7 210351AB6E75A" && level==0} -index 1
fpga -file C:/Users/ecetech/Desktop/ECE315_labs/lab1/lab_1_new/lab_1_new.sdk/design_1_wrapper_hw_platform_0/design_1_wrapper.bit
targets -set -nocase -filter {name =~"APU*" && jtag_cable_name =~ "Digilent Zybo Z7 210351AB6E75A"} -index 0
loadhw -hw C:/Users/ecetech/Desktop/ECE315_labs/lab1/lab_1_new/lab_1_new.sdk/design_1_wrapper_hw_platform_0/system.hdf -mem-ranges [list {0x40000000 0xbfffffff}]
configparams force-mem-access 1
targets -set -nocase -filter {name =~"APU*" && jtag_cable_name =~ "Digilent Zybo Z7 210351AB6E75A"} -index 0
ps7_init
ps7_post_config
targets -set -nocase -filter {name =~ "ARM*#0" && jtag_cable_name =~ "Digilent Zybo Z7 210351AB6E75A"} -index 0
dow C:/Users/ecetech/Desktop/ECE315_labs/lab1/lab_1_new/lab_1_new.sdk/part_2/Debug/part_2.elf
configparams force-mem-access 0
targets -set -nocase -filter {name =~ "ARM*#0" && jtag_cable_name =~ "Digilent Zybo Z7 210351AB6E75A"} -index 0
con
