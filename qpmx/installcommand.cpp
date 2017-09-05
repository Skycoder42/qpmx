#include "installcommand.h"

InstallCommand::InstallCommand(QObject *parent) :
	Command(parent)
{}

void InstallCommand::init(const QCliParser &parser)
{
	qDebug("Hello world");
}
