#include "command.h"
#include <QRegularExpression>

Command::Command(QObject *parent) :
	QObject(parent)
{}

void Command::finalize() {}
