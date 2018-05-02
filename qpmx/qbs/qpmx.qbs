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

	property string qpmxBin: "qpmx"
	property string qpmxDir: sourceDirectory
	readonly property string qpmxFile: FileInfo.joinPaths(qpmxDir, "qpmx.json")

	property string logLevel: "normal"
	PropertyOptions {
		name: "logLevel"
		description: "The log level the qpmx execution should use to output stuff"
		allowedValues: ["quiet", "warn-only", "normal", "verbose"]
	}
	property bool colors: true

	Depends { name: "qbs" }
	Depends { name: "Qt.core" }
	Depends { name: "qpmxdeps.global" }

	Depends {
		name: "qpmxdeps"
		condition: File.exists(qpmxFile)
		submodules: {
			var proc = new Process();
			var args = Qpmx.setBaseArgs(["qbs", "load"], qpmxDir, logLevel, colors);
			proc.exec(qpmxBin, args, true);
			var subMods = proc.readStdOut().split("\n");
			subMods.pop();
			return subMods;
		}
	}

	FileTagger {
		patterns: ["qpmx.json"]
		fileTags: "qpmx-config"
	}

	FileTagger {
		patterns: ["qpmx.user.json"]
		fileTags: "qpmx-config-user"
	}

	Rule {
		multiplex: true
		condition: qpmxdeps.global.hooks.length > 0 || qpmxdeps.global.qrcs > 0
		requiresInputs: false
		outputFileTags: ["cpp", "qpmx-startup-hooks"]
		outputArtifacts: [
			{
				filePath: "qpmx_startup_hooks.cpp",
				fileTags: ["cpp", "qpmx-startup-hooks"]
			}
		]

		prepare: {
			var command = new Command();
			command.description = "Generating qpmx startup hooks";
			command.highlight = "codegen";
			command.program = product.qpmx.qpmxBin;
			var arguments = Qpmx.setBaseArgs(["hook"], product.qpmx.qpmxDir, product.qpmx.logLevel, product.qpmx.colors);
			arguments.push("--out");
			arguments.push(output.filePath);
			for(var i = 0; i < product.qpmxdeps.global.hooks.length; i++)
				arguments.push(product.qpmxdeps.global.hooks[i]);
			arguments.push("%%");
			for(var i = 0; i < product.qpmxdeps.global.qrcs.length; i++)
				arguments.push(product.qpmxdeps.global.qrcs[i]);
			command.arguments = arguments;
			command.stdoutFilePath = product.qbs.nullDevice;
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
			fileTags: ["qm", "qpmx-qm-merged"]
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
			fileTags: ["qpmx-staticlibrary-merged"]
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
