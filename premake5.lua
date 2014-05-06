solution "KeksNL"
		configurations { "Debug KeksFramework", "Release KeksFramework" }
		
		flags { "StaticRuntime", "No64BitChecks", "Symbols" }
		nativewchar "Off"
		
		flags { "NoIncrementalLink", "NoEditAndContinue" } -- this breaks our custom section ordering in citilaunch, and is kind of annoying otherwise
		
		includedirs { "include/" }
	
		configuration "Debug*"
			targetdir "bin/debug"
			defines "NDEBUG"
			
		configuration "Release*"
			targetdir "bin/release"
			defines "NDEBUG"
			optimize "Speed"
			
		configuration "* KeksFramework"
			defines "Framework"
			
			
	project "KeksNL"
		targetname "KeksNL"
		language "C++"
		kind "ConsoleApp"
		
		vpaths { ["Header Files/*"] = "src/**.h" }
		vpaths { ["Source Files/*"] = "src/**.cpp" }
		
		files
		{
			"src/**.cpp", "include/**.h", 
			"src/Sockets/**.cpp", "include/Sockets/**.h",
		}
		
		links { "ws2_32" }
		
		defines "COMPILING_NL"	