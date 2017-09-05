#ifndef COMPILECOMMAND_H
#define COMPILECOMMAND_H

#include "command.h"

#include <QUuid>

class QtKitInfo
{
	Q_GADGET

	Q_PROPERTY(QUuid id MEMBER id)
	Q_PROPERTY(QString path MEMBER path)

	Q_PROPERTY(QVersionNumber qmakeVer MEMBER qmakeVer)
	Q_PROPERTY(QVersionNumber qtVer MEMBER qtVer)
	Q_PROPERTY(QString spec MEMBER spec)
	Q_PROPERTY(QString xspec MEMBER xspec)
	Q_PROPERTY(QString hostPrefix MEMBER hostPrefix)
	Q_PROPERTY(QString installPrefix MEMBER installPrefix)
	Q_PROPERTY(QString sysRoot MEMBER sysRoot)

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

	QList<qpmx::PackageInfo> _pkgList;
	QList<QtKitInfo> _qtKits;

	void initKits(const QStringList &qmakes);
	QtKitInfo createKit(const QString &qmakePath);
	QtKitInfo updateKit(QtKitInfo oldKit);
};

Q_DECLARE_METATYPE(QtKitInfo)

#endif // COMPILECOMMAND_H
