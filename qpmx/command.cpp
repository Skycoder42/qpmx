#include "command.h"
#include <QDateTime>
#include <QRegularExpression>

Command::Command(QObject *parent) :
	QObject(parent),
	_registry(new PluginRegistry(this))
{
	qsrand(QDateTime::currentMSecsSinceEpoch());
}

void Command::finalize() {}

PluginRegistry *Command::registry()
{
	return _registry;
}
