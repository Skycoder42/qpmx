#include "qpmxbridge_p.h"
using namespace qpmx::priv;

QpmxBridge *QpmxBridge::_instance = nullptr;

QpmxBridge::QpmxBridge() = default;

QpmxBridge::~QpmxBridge() = default;

void QpmxBridge::registerInstance(QpmxBridge *bridge)
{
	_instance = bridge;
}

QpmxBridge *QpmxBridge::instance()
{
	Q_ASSERT_X(_instance, Q_FUNC_INFO, "Cannot use libqpmx from anywhere by qpmx");
	return _instance;
}
