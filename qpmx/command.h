#ifndef COMMAND_H
#define COMMAND_H

#include <QObject>
#include <qcliparser.h>

#include "packageinfo.h"

class Command : public QObject
{
	Q_OBJECT

public:
	explicit Command(QObject *parent = nullptr);

public slots:
	virtual void initialize(const QCliParser &parser) = 0;
	virtual void finalize();
};

#endif // COMMAND_H
