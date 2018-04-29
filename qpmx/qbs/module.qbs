import qbs
import qbs.TextFile
import qbs.File
import qbs.Process

Module {
	id: qpmxModule

	property string qpmxFile: sourceDirectory + "/qpmx.json"
	property string qpmxBin: "qpmx"
	property bool recreate: false
	property bool forwardStderr: false
	property bool clean: false

	FileTagger {
		patterns: ["qpmx.json"]
		fileTags: "qpmx-config"
	}

	Probe {
		id: qpmxDepsProbe

		property alias qpmxBin: qpmxModule.qpmxBin
		property alias recreate: qpmxModule.recreate
		property alias forwardStderr: qpmxModule.forwardStderr
		property alias clean: qpmxModule.clean

		condition: File.exists(qpmxFile)
		configure: {
			var proc = new Process();
			var args = ["qbs", "init"]; //TODO set qbs path/settings dir
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
		condition: qpmxDepsProbe.found
		submodules: {
			var proc = new Process();
			var args = ["qbs", "init"]; //TODO set qbs path/settings dir
			proc.exec(qpmxBin, args, true);
			return proc.readStdOut().split("\n");
		}
	}

//	Scanner {
//		inputs: ["qpmx-config"]
//		scan: {
//			var file = new TextFile(input.filePath, TextFile.ReadOnly)
//			var json = JSON.parse(file.readAll());
//			qpmxDeps.submodules = file["dependencies"]
//			return file["dependencies"];
//		}
//	}
}
