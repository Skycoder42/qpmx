#ifndef QBSCOMMAND_H
#define QBSCOMMAND_H

#include "command.h"

class QbsCommand : public Command
{
	Q_OBJECT
public:
	explicit QbsCommand(QObject *parent = nullptr);

	QString commandName() const override;
	QString commandDescription() const override;
	QSharedPointer<QCliNode> createCliNode() const override;

protected slots:
	void initialize(QCliParser &parser) override;

private:
	QString _qbsPath;
	QDir _settingsDir;

	QList<QpmxDependency> _pkgList;
	int _pkgIndex;

	void qbsInit(const QCliParser &parser);
	void qbsGenerate(const QCliParser &parser);

	QVersionNumber findQbsVersion();
	QStringList findProfiles(const QDir &settingsDir);
	QString findQmake(const QDir &settingsDir, const QString &profile);

	void createQpmxQbs(const QDir &modRoot);
	void createQpmxGlobalQbs(const QDir &modRoot, const BuildId &kitId);

	void createNextMod(const QDir &modRoot);

	QVersionNumber readVersion(QFile &file);
	QString qbsPkgName(const QpmxDependency &dep);
};

#endif // QBSCOMMAND_H
