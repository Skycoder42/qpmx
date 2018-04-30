import qbs

Module {
	readonly property string provider: "qpm"
	readonly property string qpmxModuleName: "de.skycoder42.dialog-master"
	version: "1.3.2"

	readonly property string identity: encodeURIComponent(qpmxModuleName).replace(/\./g, "%2E").replace(/%/g, ".")
	readonly property string dependencyName : (identity + "@" + version).replace(/\./g, "_")
	readonly property string fullDependencyName : "qpmx-deps." + provider + "." + dependencyName

	Depends { name: "cpp" }
	Depends { name: "qpmx.global" }
	Depends { name: "Qt"; submodules: ["core", "gui", "widgets"]}

	readonly property string installPath: qpmx.global.cacheDir + "/" + provider + "/" + identity + "/" + version
	cpp.includePaths: [installPath + "/include"]
	cpp.libraryPaths: [installPath + "/lib"]
	cpp.staticLibraries: ["de_skycoder42_dialog-master"]
}
