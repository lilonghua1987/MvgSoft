#include "mvgsoft.h"
#include <QtWidgets/QApplication>
#include "sfm/openmvg.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	MvgSoft w;
	w.show();
	return a.exec();
}
