import qbs
import qbs.TextFile
import qbs.File
import qbs.FileInfo
import qbs.Process
import qbs.PathTools
import "qpmx.js" as Qpmx

Module {
	id: qpmxModule

	version: "%{version}"

	Depends { name: "qbs" }
	Depends { name: "Qt.core" }
	Depends { name: "qpmxdeps.global" }

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

	Rule {
		multiplex: true
		condition: qpmxdeps.global.hooks.length > 0 || qpmxdeps.global.qrcs > 0
		requiresInputs: false
		outputFileTags: ["cpp"]
		outputArtifacts: [
			{
				filePath: "qpmx_startup_hooks.cpp",
				fileTags: ["cpp"]
			}
		]

		prepare: {
			var command = new Command();
			command.description = "Generating qpmx startup hooks";
			command.highlight = "codegen";
			command.program = product.qpmx.qpmxBin;
			var arguments = Qpmx.setBaseArgs(["hook"], product.qpmx.qpmxDir, product.qpmx.logLevel, product.qpmx.colors);
			arguments.push("--out");
			arguments.push(output.filePath);//product.buildDirectory + "/" + output.fileName);
			for(var i = 0; i < product.qpmxdeps.global.hooks.length; i++)
				arguments.push(product.qpmxdeps.global.hooks[i]);
			arguments.push("%%");
			for(var i = 0; i < product.qpmxdeps.global.qrcs.length; i++)
				arguments.push(product.qpmxdeps.global.qrcs[i]);
			command.arguments = arguments;
			return command;
		}
	}

	Rule {
		inputs: ["qpmx-ts"]

		Artifact {
			filePath: input.completeBaseName + ".qm-base"
			fileTags: ["qpmx-qm-base"]
		}

		prepare: {
			var inputFilePaths = [input.filePath];
			var args = ["-silent", "-qm", output.filePath].concat(inputFilePaths);
			var cmd = new Command(product.Qt.core.binPath + "/"
								  + product.Qt.core.lreleaseName, args);
			cmd.description = "Creating " + output.fileName;
			cmd.highlight = "filegen";
			return cmd;
		}
	}

	Rule {
		inputs: ["qpmx-qm-base"]

		Artifact {
			filePath: FileInfo.joinPaths(product.Qt.core.qmDir, input.completeBaseName + ".qm")
			fileTags: ["qm"]
		}

		prepare: {
			var args = ["-if", "qm", "-i", input.filePath];
			for(var i = 0; i < product.qpmxdeps.global.qmBaseFiles.length; i++) {
				var suffix = FileInfo.completeBaseName(product.qpmxdeps.global.qmBaseFiles[i]).split("_")
				suffix.shift();
				while(suffix.length > 0) {
					if(input.baseName.endsWith(suffix.join("_")))
						break;
					suffix.shift();
				}
				if(suffix.length == 0)
					continue;

				args = args.concat(["-i", product.qpmxdeps.global.qmBaseFiles[i]]);
			}
			args = args.concat(["-of", "qm", "-o", output.filePath])
			var cmd = new Command(product.Qt.core.binPath + "/lconvert", args);
			cmd.description = "Combining translations with qpmx package translations " + output.fileName;
			cmd.highlight = "filegen";
			return cmd;
		}
	}

	Rule {
		inputs: [qbs.toolchain.contains("gcc") ? "qpmx-mri-script" : "staticlibrary"]

		Artifact {
			filePath: FileInfo.joinPaths("merged", input.completeBaseName + product.cpp.staticLibrarySuffix)
			fileTags: ["staticlibrary-merged"]
		}

		prepare: {
			//create command
			var exec = "";
			var args = [];
			if(product.qbs.toolchain.contains("msvc")) {
				exec = "lib.exe"
				args = ["/OUT:" + output.filePath, input.filePath]
				for(var i = 0; i < product.qpmxdeps.global.libdeps.length; i++)
					args.push(product.qpmxdeps.global.libdeps[i]);
			} else if(product.qbs.toolchain.contains("clang")) {
				exec = "libtool"
				args = ["-static", "-o", output.filePath, input.filePath]
				for(var i = 0; i < product.qpmxdeps.global.libdeps.length; i++)
					args.push(product.qpmxdeps.global.libdeps[i]);
			} else if(product.qbs.toolchain.contains("gcc")) {
				exec = "sh"
				args = ["-c", "ar -M < \"" + input.filePath + "\""]
			}
			var cmd = new Command(exec, args);
			cmd.description = "Merging libary with qpmx libraries";
			cmd.highlight = "linker";
			return cmd;
		}
	}

	Rule {
		inputs: ["staticlibrary"]
		condition: qbs.toolchain.contains("gcc")

		Artifact {
			filePath: FileInfo.joinPaths("merged", input.completeBaseName + ".mri")
			fileTags: ["qpmx-mri-script"]
		}

		prepare: {
			var cmd = new JavaScriptCommand();
			cmd.description = "Generating mri script to merge libary with qpmx libraries";
			cmd.highlight = "filegen";
			cmd.sourceCode = function() {
				var file = new TextFile(output.filePath, TextFile.WriteOnly);
				file.writeLine("CREATE " + FileInfo.joinPaths(FileInfo.path(input.filePath), "merged", input.completeBaseName + product.cpp.staticLibrarySuffix));
				file.writeLine("ADDLIB " + input.filePath);
				for(var i = 0; i < product.qpmxdeps.global.libdeps.length; i++)
					file.writeLine("ADDLIB " + product.qpmxdeps.global.libdeps[i]);
				file.writeLine("SAVE");
				file.writeLine("END");
				file.close();
			};
			return cmd;
		}
	}
}
