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
