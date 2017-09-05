#ifndef COMMAND_H
#define COMMAND_H

#include <QObject>
#include <qcliparser.h>

class Command : public QObject
{
	Q_OBJECT

public:
	explicit Command(QObject *parent = nullptr);

public slots:
	virtual void init(const QCliParser &parser) = 0;
};

#endif // COMMAND_H
