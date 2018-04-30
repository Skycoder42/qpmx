#include "compilecommand.h"
#include "qbscommand.h"

#include <QProcess>
#include <QStandardPaths>

QbsCommand::QbsCommand(QObject *parent) :
	Command(parent)
{}

QString QbsCommand::commandName() const
{
	return QStringLiteral("qbs");
}

QString QbsCommand::commandDescription() const
{
	return tr("Commands required to use qpmx together with qbs");
}

QSharedPointer<QCliNode> QbsCommand::createCliNode() const
{
	auto qbsNode = QSharedPointer<QCliContext>::create();
	qbsNode->addOption({
						   QStringLiteral("path"),
						   tr("The <path> to the qbs executable to be used. "
						   "If not specified the environments default is used."),
						   QStringLiteral("path"),
						   QStandardPaths::findExecutable(QStringLiteral("qbs"))
					   });
	qbsNode->addOption({
						   {QStringLiteral("s"), QStringLiteral("settings-dir")},
						   tr("Passed as --settings-dir <dir> to the qbs executable. "
						   "This way custom settings directories can be specified."),
						   QStringLiteral("dir")
					   });

	auto initNode = qbsNode->addLeafNode(QStringLiteral("init"),
										 tr("Initialize the current project by installing, compiling and preparing packages for qbs."));
	initNode->addOption({
							QStringLiteral("r"),
							tr("Pass the -r option to the install, compile and generate commands.")
						});
	initNode->addOption({
							{QStringLiteral("e"), QStringLiteral("stderr")},
							tr("Pass the --stderr cli flag to to compile step. This will forward stderr of "
							   "subprocesses instead of logging to a file.")
						});
	initNode->addOption({
							{QStringLiteral("c"), QStringLiteral("clean")},
							tr("Pass the --clean cli flag to the compile step. This will generate clean dev "
							   "builds instead of caching them for speeding builds up.")
						});
	initNode->addOption({
							{QStringLiteral("p"), QStringLiteral("profile")},
							tr("Pass all as --profile cli flags to to generate step. "
							"This determines for which <profiles> the qpmx qbs modules should be created."),
							QStringLiteral("profile")
						});
	initNode->addOption({
							QStringLiteral("qbs-version"),
							tr("The <version> of the qbs that is used. Is needed to find the correct settings. "
							"If not specified, qbs is invoked to find it out."),
							QStringLiteral("version")
						});

	auto generateNode = qbsNode->addLeafNode(QStringLiteral("generate"),
											 tr("Generate the qbs modules for all given packages."));
	generateNode->addOption({
								{QStringLiteral("r"), QStringLiteral("recreate")},
								tr("Always delete and recreate the files if they exist, not only when the configuration changed.")
						   });
	generateNode->addOption({
								{QStringLiteral("p"), QStringLiteral("profile")},
								tr("The <profiles> to generate the qpmx qbs modules for. "
								"Can be specified multiple times to create for multiple modules."),
								QStringLiteral("profile")
							});
	generateNode->addOption({
								QStringLiteral("qbs-version"),
								tr("The <version> of the qbs that is used. Is needed to find the correct settings. "
								"If not specified, qbs is invoked to find it out."),
								QStringLiteral("version")
							});

	auto loadNode = qbsNode->addLeafNode(QStringLiteral("load"),
										 tr("Load the names of all top level qbs qpmx modules that the qpmx.json file requires."));

	return qbsNode;
}

void QbsCommand::initialize(QCliParser &parser)
{
	try {
		// qbs path
		_qbsPath = parser.value(QStringLiteral("path"));
		xDebug() << tr("Using qbs executable: %1").arg(_qbsPath);

		// find the settings dir
		if(parser.isSet(QStringLiteral("settings-dir")))
			_settingsDir = parser.value(QStringLiteral("settings-dir"));
		else {
			_settingsDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
			_settingsDir.cd(QStringLiteral("QtProject/qtcreator"));
		}

		if(parser.enterContext(QStringLiteral("init")))
			qbsInit(parser);
		else if(parser.enterContext(QStringLiteral("generate")))
			qbsGenerate(parser);
		else
			Q_UNREACHABLE();
		qApp->quit();
	} catch (QString &s) {
		xCritical() << s;
	}
}

void QbsCommand::qbsInit(const QCliParser &parser)
{
	//get the version
	QVersionNumber qbsVersion;
	if(parser.isSet(QStringLiteral("qbs-version")))
		qbsVersion = QVersionNumber::fromString(parser.value(QStringLiteral("qbs-version")));
	else
		qbsVersion = findQbsVersion();

	// get the profile dir
	auto profileDir = _settingsDir;
	if(!profileDir.cd(QStringLiteral("qbs")) ||
	   !profileDir.cd(qbsVersion.toString()))
		throw tr("Unabled to find settings directory. Specify it explicitly via --settings-dir");
	xDebug() << tr("Using qbs settings path: %1").arg(profileDir.path());

	auto profiles = parser.values(QStringLiteral("profile"));
	if(profiles.isEmpty())
		profiles = findProfiles(profileDir);

	auto reRun = parser.isSet(QStringLiteral("r"));

	xDebug() << tr("Running for following qbs profiles: %1").arg(profiles.join(tr(", ")));
	for(const auto &profile : qAsConst(profiles)) {
		xInfo() << tr("Running qbs init for qbs profile %{bld}%1%{end}").arg(profile);

		//run install
		QStringList runArgs {
			QStringLiteral("install")
		};
		if(reRun)
			runArgs.append(QStringLiteral("--renew"));
		subCall(runArgs);
		xDebug() << tr("Successfully ran install step for");

		//run compile
		auto qmake = findQmake(profileDir, profile);
		runArgs = QStringList {
			QStringLiteral("compile"),
			QStringLiteral("--qmake"),
			qmake
		};
		if(reRun)
			runArgs.append(QStringLiteral("--recompile"));
		if(parser.isSet(QStringLiteral("stderr")))
			runArgs.append(QStringLiteral("--stderr"));
		if(parser.isSet(QStringLiteral("clean")))
			runArgs.append(QStringLiteral("--clean"));
		subCall(runArgs);
		xDebug() << tr("Successfully ran compile step");

		//run generate
		runArgs = QStringList {
			QStringLiteral("qbs"),
			QStringLiteral("generate"),
			QStringLiteral("--path"),
			_qbsPath,
			QStringLiteral("--settings-dir"),
			_settingsDir.absolutePath(),
			QStringLiteral("--profile"),
			profile,
			QStringLiteral("--qbs-version"),
			qbsVersion.toString(),
		};
		if(reRun)
			runArgs.append(QStringLiteral("--recreate"));
		subCall(runArgs);
		xDebug() << tr("Successfully ran qbs generate step");
	}

	xDebug() << tr("Completed qpmx qbs initialization");
}

void QbsCommand::qbsGenerate(const QCliParser &parser)
{
	//get the version
	QVersionNumber qbsVersion;
	if(parser.isSet(QStringLiteral("qbs-version")))
		qbsVersion = QVersionNumber::fromString(parser.value(QStringLiteral("qbs-version")));
	else
		qbsVersion = findQbsVersion();

	// get the profile dir
	auto profileDir = _settingsDir;
	if(!profileDir.cd(QStringLiteral("qbs")) ||
	   !profileDir.cd(qbsVersion.toString()))
		throw tr("Unabled to find settings directory. Specify it explicitly via --settings-dir");
	xDebug() << tr("Using qbs settings path: %1").arg(profileDir.path());

	auto profiles = parser.values(QStringLiteral("profile"));
	if(profiles.isEmpty())
		profiles = findProfiles(profileDir);

	xDebug() << tr("Running for following qbs profiles: %1").arg(profiles.join(tr(", ")));
	auto format = QpmxUserFormat::readDefault(true);
	if(format.hasDevOptions())
		throw tr("For now, qbs builds do not support the dev mode!");
	auto recreate = parser.isSet(QStringLiteral("recreate"));

	for(const auto &profile : qAsConst(profiles)) {
		xDebug() << tr("Running qbs generate for qbs profile %{bld}%1%{end}").arg(profile);

		auto qmake = findQmake(profileDir, profile);
		BuildId kitId;
		if(format.source)
			kitId = QStringLiteral("src");
		else
			kitId = QtKitInfo::findKitId(buildDir(), qmake);

		//create meta files
		auto pDir = profileDir;
		if(!pDir.cd(QStringLiteral("profiles")) ||
		   !pDir.cd(profile) ||
		   !pDir.cd(QStringLiteral("modules")))
			throw tr("Unable to find qbs modules folder for profile %{bld}%1%{end}").arg(profile);

		if(pDir.exists(QStringLiteral("qpmx")) && recreate) {
			auto rmDir = pDir;
			if(!rmDir.cd(QStringLiteral("qpmx")) ||
			   !rmDir.removeRecursively())
				throw tr("Failed to remove old qpmx module for profile %{bld}%1%{end}").arg(profile);
			xDebug() << tr("Removed old qpmx module");
		}
		if(pDir.exists(QStringLiteral("qpmx-deps")) && recreate) {
			auto rmDir = pDir;
			if(!rmDir.cd(QStringLiteral("qpmx-deps")) ||
			   !rmDir.removeRecursively())
				throw tr("Failed to remove old qpmx-deps module dir for profile %{bld}%1%{end}").arg(profile);
			xDebug() << tr("Removed old qpmx-deps module dir");
		}
		createQpmxQbs(pDir);
		createQpmxGlobalQbs(pDir, kitId);

		// create the actual qbs modules for packages
		_pkgList = format.dependencies;
		for(_pkgIndex = 0; _pkgIndex < _pkgList.size(); _pkgIndex++)
			createNextMod(pDir);
	}
}

QVersionNumber QbsCommand::findQbsVersion()
{
	QProcess p;
	p.setProgram(_qbsPath);
	p.setArguments({QStringLiteral("--version")});
	p.start();
	p.waitForFinished(-1);

	if(p.error() != QProcess::UnknownError)
		throw tr("Failed to run qbs subprocess with process error: %1").arg(p.errorString());
	if(p.exitStatus() != QProcess::NormalExit)
		throw tr("Failed to run qbs subprocess - it crashed");
	else if(p.exitCode() != EXIT_SUCCESS) {
		_ExitCode = p.exitCode();
		throw tr("qbs subprocess failed.");
	}

	auto version = QVersionNumber::fromString(QString::fromUtf8(p.readAllStandardOutput()));
	xDebug() << tr("Detected qbs version as %1").arg(version.toString());
	return version;
}

QStringList QbsCommand::findProfiles(const QDir &settingsDir)
{
	auto pDir = settingsDir;
	if(!pDir.cd(QStringLiteral("profiles")))
		return {};
	else
		return pDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
}

QString QbsCommand::findQmake(const QDir &settingsDir, const QString &profile)
{
	//extract the qmake from the profiles QtCore module
	auto pDir = settingsDir;
	if(!pDir.cd(QStringLiteral("profiles")) ||
	   !pDir.cd(profile) ||
	   !pDir.cd(QStringLiteral("modules/Qt/core")))
		throw tr("Unable to find Qt.core qbs module for profile %{bld}%1%{end}").arg(profile);

	// extract the Qt bin path from qbs
	QFile qbsFile(pDir.absoluteFilePath(QStringLiteral("core.qbs")));
	if(!qbsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
		throw tr("Failed to open %1 with error: %2")
				.arg(qbsFile.fileName(), qbsFile.errorString());
	}
	QTextStream stream(&qbsFile);
	static const QRegularExpression binRegex {
		QStringLiteral(R"__(property path binPath: "([^"]*)")__"),
		QRegularExpression::OptimizeOnFirstUsageOption
	};
	while(!stream.atEnd()) {
		auto line = stream.readLine().trimmed();
		auto match = binRegex.match(line);
		if(match.hasMatch()) {
			auto binPath = match.captured(1);
			auto qmakePath = QStandardPaths::findExecutable(QStringLiteral("qmake"), {binPath});
			xDebug() << tr("Detected qmake for profile %{bld}%1%{end} as: %2")
						.arg(profile, qmakePath);
			return qmakePath;
		}
	}

	throw tr("Unable to find property binPath in core.qbs");
}

void QbsCommand::createQpmxQbs(const QDir &modRoot)
{
	auto appVer = QVersionNumber::fromString(QCoreApplication::applicationVersion());

	auto modDir = modRoot;
	if(!modDir.mkpath(QStringLiteral("qpmx")) ||
	   !modDir.cd(QStringLiteral("qpmx")))
		throw tr("Failed to create qpmx module dir in: %1").arg(modDir.path());
	QFile outFile(modDir.absoluteFilePath(QStringLiteral("module.qbs")));
	if(outFile.exists() && appVer == readVersion(outFile)) {
		xDebug() << tr("qpmx qbs module already exists");
		return;
	}

	xDebug() << QStringLiteral("qpmx qbs module needs to be created or updated");
	QFile inFile(QStringLiteral(":/build/qbs/qpmx.qbs"));
	if(!inFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
		throw tr("Failed to open %1 with error: %2")
				.arg(inFile.fileName(), inFile.errorString());
	}

	auto content = inFile.readAll();
	inFile.close();
	content.replace("%{version}", appVer.toString().toUtf8());

	if(!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
		throw tr("Failed to open %1 with error: %2")
				.arg(outFile.fileName(), outFile.errorString());
	}
	outFile.write(content);
	outFile.close();
	xDebug() << tr("Created qpmx qbs module");
}

void QbsCommand::createQpmxGlobalQbs(const QDir &modRoot, const BuildId &kitId)
{
	auto modDir = modRoot;
	if(!modDir.mkpath(QStringLiteral("qpmx/global")) ||
	   !modDir.cd(QStringLiteral("qpmx/global")))
		throw tr("Failed to create qpmx module dir in: %1").arg(modDir.path());

	// check if kit id matches
	if(!modDir.exists(kitId)) {
		if(!modDir.removeRecursively() ||
		   !modDir.mkpath(QStringLiteral(".")))
			throw tr("Failed to recreate qpmx.global qbs module: %1").arg(modDir.path());
	} else {
		xDebug() << tr("qpmx.global qbs module already exists");
		return;
	}

	QFile outFile(modDir.absoluteFilePath(QStringLiteral("module.qbs")));
	if(!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
		throw tr("Failed to open %1 with error: %2")
				.arg(outFile.fileName(), outFile.errorString());
	}
	QTextStream stream(&outFile);
	stream << "import qbs\n\n"
		   << "Module {\n"
		   << "\tversion: \"" << QCoreApplication::applicationVersion() << "\"\n"
		   << "\treadonly property string cacheDir: \"" << buildDir(kitId).absolutePath() << "\"\n"
		   << "}\n";
	stream.flush();
	outFile.close();

	QFile kitFile(modDir.absoluteFilePath(kitId));
	if(!kitFile.open(QIODevice::WriteOnly)) {
		xWarning() <<  tr("Failed to open %1 with error: %2")
					   .arg(kitFile.fileName(), kitFile.errorString());
	}
	xDebug() << tr("Created qpmx.global qbs module");
}

void QbsCommand::createNextMod(const QDir &modRoot)
{
	auto dep = _pkgList.value(_pkgIndex);
	auto depName = qbsPkgName(dep);
	QString modPath = QStringLiteral("qpmx-deps/") + dep.provider + QLatin1Char('/') + depName;

	auto modDir = modRoot;
	if(!modDir.mkpath(modPath) ||
	   !modDir.cd(modPath))
		throw tr("Failed to create qpmx module dir in: %1").arg(modDir.path());

	// load src qpmx.json
	auto srcFmDir = srcDir(dep);
	auto srcFormat = QpmxFormat::readFile(srcFmDir, true);

	// generate the qbs file
	QFile qbsMod(modDir.absoluteFilePath(QStringLiteral("QpmxModule.qbs")));
	if(qbsMod.exists()) {
		xDebug() << tr("qbs module for package %1 already exists").arg(dep.toString());
		return;
	}
	if(!qbsMod.open(QIODevice::WriteOnly | QIODevice::Text)) {
		throw tr("Failed to open %1 with error: %2")
				.arg(qbsMod.fileName(), qbsMod.errorString());
	}
	QTextStream stream(&qbsMod);
	stream << "import qbs\n\n"
		   << "Module {\n"
		   << "\treadonly property string provider: \"" << dep.provider << "\"\n"
		   << "\treadonly property string qpmxModuleName: \"" << dep.package << "\"\n"
		   << "\tversion: \"" << dep.version.toString() << "\"\n\n";
	// fixed part
	stream << '\t' << R"__(readonly property string identity: encodeURIComponent(qpmxModuleName).replace(/\./g, "%2E").replace(/%/g, "."))__" << '\n'
		   << '\t' << R"__(readonly property string dependencyName : (identity + "@" + version).replace(/\./g, "_"))__" << '\n'
		   << '\t' << R"__(readonly property string fullDependencyName : "qpmx-deps." + provider + "." + dependencyName)__" << "\n\n"
		   << "\tDepends { name: \"cpp\" }\n"
		   << "\tDepends { name: \"qpmx.global\" }\n";
	// dependencies
	for(const auto &dependency : srcFormat.dependencies) {
		if(!_pkgList.contains(dependency)) //TODO take version into account
			_pkgList.append(dependency);
		stream << "\tDepends { name: \"qpmx-deps." << dependency.provider << '.' << qbsPkgName(dependency) << "\" }\n";
	}
	// includes and the lib
	auto libName = QFileInfo(srcFormat.priFile).completeBaseName();
	stream << "\n\t" << R"__(readonly property string installPath: qpmx.global.cacheDir + "/" + provider + "/" + identity + "/" + version)__" << '\n'
		   << "\tcpp.includePaths: [installPath + \"/include\"]\n"
		   << "\tcpp.libraryPaths: [installPath + \"/lib\"]\n"
		   << "\tcpp.staticLibraries: [qbs.debugInformation ? \"" << libName << "d\" : \"" << libName << "\"]\n"
		   << "}";
	stream.flush();
	qbsMod.close();

	//check if prc is given
	if(!srcFormat.qbsFile.isEmpty()) {
		if(!QFile::copy(srcFmDir.absoluteFilePath(srcFormat.qbsFile),
					modDir.absoluteFilePath(QStringLiteral("module.qbs"))))
			throw tr("Failed to copy qbs file to qbs module dir for package %1").arg(dep.toString());
	} else {
		if(!qbsMod.rename(modDir.absoluteFilePath(QStringLiteral("module.qbs"))))
			throw tr("Failed to generate qbs file for package %1").arg(dep.toString());
	}
	xInfo() << tr("Created qbs module for package %1").arg(dep.toString());
}

QVersionNumber QbsCommand::readVersion(QFile &file)
{
	if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		throw tr("Failed to open %1 with error: %2")
				.arg(file.fileName(), file.errorString());
	}
	QTextStream stream(&file);
	static const QRegularExpression binRegex {
		QStringLiteral(R"__(version: "((?:\d|\.)*)")__"),
		QRegularExpression::OptimizeOnFirstUsageOption
	};
	while(!stream.atEnd()) {
		auto line = stream.readLine().trimmed();
		auto match = binRegex.match(line);
		if(match.hasMatch()) {
			auto version = match.captured(1);
			file.close();
			return QVersionNumber::fromString(version);
		}
	}

	throw tr("Unable to find property binPath in core.qbs");
}

QString QbsCommand::qbsPkgName(const QpmxDependency &dep)
{
	return QString{pkgEncode(dep.package) + QLatin1Char('@') + dep.version.toString()}.replace(QLatin1Char('.'), QLatin1Char('_'));
}
