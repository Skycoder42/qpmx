import qbs
import qbs.TextFile
import qbs.File
import qbs.Process
import "qpmx.js" as Qpmx

Module {
	id: qpmxModule

	version: "%{version}"

	Depends { name: "qbs" }

	property string qpmxDir: sourceDirectory
	property string qpmxBin: "qpmx"
	property bool autoProbe: false

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

	FileTagger {
		patterns: ["qpmx.json", "qpmx.user.json"]
		fileTags: "qpmx-config"
	}

	Probe {
		id: qpmxDepsProbe

		condition: autoProbe && File.exists(qpmxFile)
		configure: {
			var proc = new Process();
			var args = Qpmx.setBaseArgs(["qbs", "init"], qpmxModule.qpmxDir, qpmxModule.logLevel, qpmxModule.colors);
			qbs.profiles.forEach(function(profile) {
				args.push("--profile");
				args.push(profile);
			});
			args.push("--qbs-version");
			args.push(qbs.version);
			if(qpmxModule.recreate)
				args.push("-r");
			if(qpmxModule.forwardStderr)
				args.push("--stderr");
			if(qpmxModule.clean)
				args.push("--clean");
			proc.exec(qpmxModule.qpmxBin, args, true);
			found = true;
		}
	}

	Depends {
		name: "qpmxdeps"
		condition: File.exists(qpmxFile) && (!autoProbe || qpmxDepsProbe.found)
		submodules: {
			var proc = new Process();
			var args = Qpmx.setBaseArgs(["qbs", "load"], qpmxDir, logLevel, colors);
			proc.exec(qpmxBin, args, true);
			var subMods = proc.readStdOut().split("\n");
			subMods.pop();
			return subMods;
		}
	}
}
