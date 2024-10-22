
open_project pdp8l.xpr
update_compile_order -fileset sources_1
update_module_reference myboard_Zynq_0_0

reset_run synth_1
launch_runs impl_1 -to_step write_bitstream -jobs 8

