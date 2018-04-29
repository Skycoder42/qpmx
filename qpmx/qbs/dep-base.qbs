import qbs

Module {
	readonly property string provider: "qpm"
	readonly property string qpmxModuleName: "de.skycoder42.dialog-master"
	readonly property string identity: encodeURIComponent(qpmxModuleName).replace(/\./g, "%2E").replace(/%/g, ".")
	version: "1.3.2"

	Depends { name: "cpp" }
	Depends { name: "Qt"; submodules: ["core", "gui", "widgets"]}
	Depends { name: "qpmxglobal" }

	readonly property string installPath: qpmxglobal.cacheDir + "/" + provider + "/" + identity + "/" + version

	cpp.includePaths: [installPath + "/include"]
	cpp.libraryPaths: [installPath + "/lib"]
	cpp.staticLibraries: ["de_skycoder42_dialog-master"]
}
