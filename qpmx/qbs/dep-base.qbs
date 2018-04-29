import qbs 1.0

Module {
	property string qpmxModuleName: "dialog-master"
	version: "1.3.2"

	Depends { name: "cpp" }
	Depends { name: "Qt"; submodules: ["core", "gui", "widgets"]}

	cpp.includePaths: ["/home/sky/.cache/Skycoder42/qpmx/build/dc9777cf-a22f-4464-b4c9-6c9c505b9151/qpm/de.2Eskycoder42.2Edialog-master/1.3.2/include/"]
	cpp.libraryPaths: ["/home/sky/.cache/Skycoder42/qpmx/build/dc9777cf-a22f-4464-b4c9-6c9c505b9151/qpm/de.2Eskycoder42.2Edialog-master/1.3.2/lib/"]
	cpp.staticLibraries: ["de_skycoder42_dialog-master"]
}
