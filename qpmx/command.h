#ifndef COMMAND_H
#define COMMAND_H

#include <QObject>
#include <QDebug>
#include <qcliparser.h>

#include "packageinfo.h"
#include "pluginregistry.h"

class Command : public QObject
{
	Q_OBJECT

public:
	explicit Command(QObject *parent = nullptr);

public slots:
	virtual void initialize(QCliParser &parser) = 0;
	virtual void finalize();

protected:
	PluginRegistry *registry();

private:
	PluginRegistry *_registry;
};

#define xDebug(...) qDebug(__VA_ARGS__).noquote()
#define xInfo(...) qInfo(__VA_ARGS__).noquote()
#define xWarning(...) qWarning(__VA_ARGS__).noquote()
#define xCritical(...) qCritical(__VA_ARGS__).noquote()

#endif // COMMAND_H
