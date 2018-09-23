#ifndef INSTALLCOMMAND_H
#define INSTALLCOMMAND_H

#include "command.h"
#include "qpmxformat.h"
#include <QTemporaryDir>

class InstallCommand : public Command
{
	Q_OBJECT

public:
	explicit InstallCommand(QObject *parent = nullptr);

	QString commandName() const override;
	QString commandDescription() const override;
	QSharedPointer<QCliNode> createCliNode() const override;

protected slots:
	void initialize(QCliParser &parser) override;

private:
	bool _renew = false;
	bool _noPrepare = false;

	QList<QpmxDevDependency> _pkgList;
	QList<QpmxDevAlias> _aliases;
	int _addPkgCount = 0;

	void getPackages();
	void completeInstall();

	bool getVersion(QpmxDevDependency &current, qpmx::SourcePlugin *plugin, bool mustWork);

	bool installPackage(QpmxDevDependency &current, qpmx::SourcePlugin *plugin, bool mustWork);
	bool getSource(QpmxDevDependency &current, qpmx::SourcePlugin *plugin, bool mustWork, CacheLock &lock);
	void createSrcInclude(const QpmxDevDependency &current, const QpmxFormat &format, const CacheLock &lock);

	void verifyDeps(const QList<QpmxDevDependency> &list, const QpmxDevDependency &base) const;
	void detectDeps(const QpmxFormat &format);

};

#endif // INSTALLCOMMAND_H
