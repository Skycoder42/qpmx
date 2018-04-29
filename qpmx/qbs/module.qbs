import qbs
import qbs.TextFile
import qbs.File
import qbs.Process

Module {
	id: qpmxModule

	version: 1.5.0 //TODO always update

	Depends { name: "qbs" }

	property string qpmxDir: sourceDirectory
	property string qpmxBin: "qpmx"
	property bool autoProbe: true

	// general params
	property string logLevel: "normal"
	PropertyOptions {
		name: "logLevel"
		description: "The log level the qpmx execution should use to output stuff"
		allowedValues: ["quiet", "warn-only", "normal", "verbose"]
	}
	property bool colors: true
	property string devCache: ""
	// probe params
	property bool recreate: false
	property bool forwardStderr: false
	property bool clean: false

	readonly property string qpmxFile: qpmxDir + "/qpmx.json"

	function setBaseArgs(args) {
		args.push("--dir");
		args.push(qpmxDir);
		switch(logLevel) {
		case "quiet":
			args.push("--quiet");
			break;
		case "warn-only":
			args.push("--quiet");
			args.push("--verbose");
			break;
		case "normal":
			break;
		case "verbose":
			args.push("--verbose");
			break;
		}
		if(!colors)
			args.push("--no-color");
		if(devCache != "") {
			args.push("--dev-cache");
			args.push(devCache);
		}
		return args;
	}

	FileTagger {
		patterns: ["qpmx.json", "qpmx.user.json"]
		fileTags: "qpmx-config"
	}

	Probe {
		id: qpmxDepsProbe

		readonly property alias qpmxBin: qpmxModule.qpmxBin
		readonly property alias recreate: qpmxModule.recreate
		readonly property alias forwardStderr: qpmxModule.forwardStderr
		readonly property alias clean: qpmxModule.clean
		readonly property var profiles : qbs.profiles

		condition: autoProbe && File.exists(qpmxFile)
		configure: {
			var proc = new Process();
			var args = setBaseArgs(["qbs", "init"]);
			profiles.forEach(function(profile) {
				args.push("--profile");
				args.push(profile);
			});
			args.push("--qbs-version");
			args.push(qbs.version);
			if(recreate)
				args.push("-r");
			if(forwardStderr)
				args.push("--stderr");
			if(clean)
				args.push("--clean");
			proc.exec(qpmxBin, args, true);
			found = true;
		}
	}

	Depends {
		id: qpmxDeps
		name: "qpmx-deps"
		condition: File.exists(qpmxFile) && (!autoProbe || qpmxDepsProbe.found)
		submodules: {
			var proc = new Process();
			var args = setBaseArgs(["qbs", "init"]);
			proc.exec(qpmxBin, args, true);
			return proc.readStdOut().split("\n");
		}
	}
}
