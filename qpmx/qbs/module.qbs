import qbs
import qbs.TextFile

Module {
	property string qpmxFile: sourceDirectory + "/qpmx.json"

	FileTagger {
		patterns: ["qpmx.json"]
		fileTags: "qpmx-config"
	}

	Depends {
		id: qpmxDeps
		name: "qpmx-deps"
		submodules: {
			var file = new TextFile(qpmxFile, TextFile.ReadOnly)
			var json = JSON.parse(file.readAll());
			var depRes = []
			json["dependencies"].forEach(function(item) {
				depRes.push(item["package"] + "@" + item["version"].split(".").join("_"));
			});
			return depRes;
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
