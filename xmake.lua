add_rules("mode.debug", "mode.release")

target("retrogfx")
	set_kind("binary")
	add_cxflags("-std=c++20")
	add_links("raylib", "boost_program_options")
	add_files("src/*.cpp")
	set_rundir(".")
	if is_mode("release") then
		set_optimize("fastest")
	end

