#ifndef COMPILECOMMAND_H
#define COMPILECOMMAND_H

#include "command.h"

#include <QUuid>
#include <QSettings>

class QtKitInfo
{
public:
	QtKitInfo(const QString &path = {});

	operator bool() const;
	bool operator ==(const QtKitInfo &other) const;

	QUuid id;
	QString path;

	QVersionNumber qmakeVer;
	QVersionNumber qtVer;
	QString spec;
	QString xspec;
	QString hostPrefix;
	QString installPrefix;
	QString sysRoot;
};

class CompileCommand : public Command
{
	Q_OBJECT

public:
	explicit CompileCommand(QObject *parent = nullptr);

public slots:
	void initialize(QCliParser &parser) override;

private:
	bool _global;
	bool _recompile;
	QSettings *_settings;

	QList<qpmx::PackageInfo> _pkgList;
	QList<QtKitInfo> _qtKits;

	void initKits(const QStringList &qmakes);
	QtKitInfo createKit(const QString &qmakePath);
	QtKitInfo updateKit(QtKitInfo oldKit, bool mustWork);
};

#endif // COMPILECOMMAND_H
