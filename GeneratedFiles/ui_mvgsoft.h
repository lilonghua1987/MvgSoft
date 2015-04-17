/********************************************************************************
** Form generated from reading UI file 'mvgsoft.ui'
**
** Created by: Qt User Interface Compiler version 5.3.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MVGSOFT_H
#define UI_MVGSOFT_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MvgSoftClass
{
public:
    QAction *actionFiles;
    QAction *actionComputeFocal;
    QAction *actionIncremental;
    QAction *actionGlobal;
    QAction *actionEMatch;
    QAction *actionFMatch;
    QAction *actionHMatch;
    QAction *actionCMPMVS;
    QAction *actionPMVS;
    QAction *actionMeshLab;
    QAction *actionCMVS;
    QAction *actionPMVS2;
    QAction *actionMesh;
    QAction *actionLibViso;
    QAction *actionColor;
    QWidget *centralWidget;
    QStatusBar *statusBar;
    QMenuBar *menuBar;
    QMenu *menuOpen;
    QMenu *menuTools;
    QMenu *menuMatche;
    QMenu *menuPipeLine;
    QMenu *menuReconstruct;

    void setupUi(QMainWindow *MvgSoftClass)
    {
        if (MvgSoftClass->objectName().isEmpty())
            MvgSoftClass->setObjectName(QStringLiteral("MvgSoftClass"));
        MvgSoftClass->resize(600, 400);
        actionFiles = new QAction(MvgSoftClass);
        actionFiles->setObjectName(QStringLiteral("actionFiles"));
        QIcon icon;
        icon.addFile(QStringLiteral(":/MvgSoft/Images"), QSize(), QIcon::Normal, QIcon::Off);
        actionFiles->setIcon(icon);
        actionComputeFocal = new QAction(MvgSoftClass);
        actionComputeFocal->setObjectName(QStringLiteral("actionComputeFocal"));
        QIcon icon1;
        icon1.addFile(QStringLiteral(":/MvgSoft/Focal"), QSize(), QIcon::Normal, QIcon::Off);
        actionComputeFocal->setIcon(icon1);
        actionIncremental = new QAction(MvgSoftClass);
        actionIncremental->setObjectName(QStringLiteral("actionIncremental"));
        QIcon icon2;
        icon2.addFile(QStringLiteral(":/MvgSoft/incremental"), QSize(), QIcon::Normal, QIcon::Off);
        actionIncremental->setIcon(icon2);
        actionGlobal = new QAction(MvgSoftClass);
        actionGlobal->setObjectName(QStringLiteral("actionGlobal"));
        QIcon icon3;
        icon3.addFile(QStringLiteral(":/MvgSoft/global"), QSize(), QIcon::Normal, QIcon::Off);
        actionGlobal->setIcon(icon3);
        actionEMatch = new QAction(MvgSoftClass);
        actionEMatch->setObjectName(QStringLiteral("actionEMatch"));
        QIcon icon4;
        icon4.addFile(QStringLiteral(":/MvgSoft/eMatrix"), QSize(), QIcon::Normal, QIcon::Off);
        actionEMatch->setIcon(icon4);
        actionFMatch = new QAction(MvgSoftClass);
        actionFMatch->setObjectName(QStringLiteral("actionFMatch"));
        QIcon icon5;
        icon5.addFile(QStringLiteral(":/MvgSoft/fMatrix"), QSize(), QIcon::Normal, QIcon::Off);
        actionFMatch->setIcon(icon5);
        actionHMatch = new QAction(MvgSoftClass);
        actionHMatch->setObjectName(QStringLiteral("actionHMatch"));
        QIcon icon6;
        icon6.addFile(QStringLiteral(":/MvgSoft/hMatrix"), QSize(), QIcon::Normal, QIcon::Off);
        actionHMatch->setIcon(icon6);
        actionCMPMVS = new QAction(MvgSoftClass);
        actionCMPMVS->setObjectName(QStringLiteral("actionCMPMVS"));
        QIcon icon7;
        icon7.addFile(QStringLiteral(":/MvgSoft/CMPMVS"), QSize(), QIcon::Normal, QIcon::Off);
        actionCMPMVS->setIcon(icon7);
        actionPMVS = new QAction(MvgSoftClass);
        actionPMVS->setObjectName(QStringLiteral("actionPMVS"));
        QIcon icon8;
        icon8.addFile(QStringLiteral(":/MvgSoft/PMVS"), QSize(), QIcon::Normal, QIcon::Off);
        actionPMVS->setIcon(icon8);
        actionMeshLab = new QAction(MvgSoftClass);
        actionMeshLab->setObjectName(QStringLiteral("actionMeshLab"));
        QIcon icon9;
        icon9.addFile(QStringLiteral(":/MvgSoft/meshlab"), QSize(), QIcon::Normal, QIcon::Off);
        actionMeshLab->setIcon(icon9);
        actionCMVS = new QAction(MvgSoftClass);
        actionCMVS->setObjectName(QStringLiteral("actionCMVS"));
        QIcon icon10;
        icon10.addFile(QStringLiteral(":/MvgSoft/CMVS"), QSize(), QIcon::Normal, QIcon::Off);
        actionCMVS->setIcon(icon10);
        actionPMVS2 = new QAction(MvgSoftClass);
        actionPMVS2->setObjectName(QStringLiteral("actionPMVS2"));
        QIcon icon11;
        icon11.addFile(QStringLiteral(":/MvgSoft/PMVS2"), QSize(), QIcon::Normal, QIcon::Off);
        actionPMVS2->setIcon(icon11);
        actionMesh = new QAction(MvgSoftClass);
        actionMesh->setObjectName(QStringLiteral("actionMesh"));
        QIcon icon12;
        icon12.addFile(QStringLiteral(":/MvgSoft/Mesh"), QSize(), QIcon::Normal, QIcon::Off);
        actionMesh->setIcon(icon12);
        actionLibViso = new QAction(MvgSoftClass);
        actionLibViso->setObjectName(QStringLiteral("actionLibViso"));
        QIcon icon13;
        icon13.addFile(QStringLiteral(":/MvgSoft/LibViso"), QSize(), QIcon::Normal, QIcon::Off);
        actionLibViso->setIcon(icon13);
        actionColor = new QAction(MvgSoftClass);
        actionColor->setObjectName(QStringLiteral("actionColor"));
        QIcon icon14;
        icon14.addFile(QStringLiteral(":/MvgSoft/ColorHarm"), QSize(), QIcon::Normal, QIcon::Off);
        actionColor->setIcon(icon14);
        centralWidget = new QWidget(MvgSoftClass);
        centralWidget->setObjectName(QStringLiteral("centralWidget"));
        MvgSoftClass->setCentralWidget(centralWidget);
        statusBar = new QStatusBar(MvgSoftClass);
        statusBar->setObjectName(QStringLiteral("statusBar"));
        MvgSoftClass->setStatusBar(statusBar);
        menuBar = new QMenuBar(MvgSoftClass);
        menuBar->setObjectName(QStringLiteral("menuBar"));
        menuBar->setGeometry(QRect(0, 0, 600, 23));
        menuOpen = new QMenu(menuBar);
        menuOpen->setObjectName(QStringLiteral("menuOpen"));
        QIcon icon15;
        icon15.addFile(QStringLiteral(":/MvgSoft/openFile"), QSize(), QIcon::Normal, QIcon::Off);
        menuOpen->setIcon(icon15);
        menuTools = new QMenu(menuBar);
        menuTools->setObjectName(QStringLiteral("menuTools"));
        QIcon icon16;
        icon16.addFile(QStringLiteral(":/MvgSoft/Tools"), QSize(), QIcon::Normal, QIcon::Off);
        menuTools->setIcon(icon16);
        menuMatche = new QMenu(menuTools);
        menuMatche->setObjectName(QStringLiteral("menuMatche"));
        QIcon icon17;
        icon17.addFile(QStringLiteral(":/MvgSoft/match"), QSize(), QIcon::Normal, QIcon::Off);
        menuMatche->setIcon(icon17);
        menuPipeLine = new QMenu(menuTools);
        menuPipeLine->setObjectName(QStringLiteral("menuPipeLine"));
        QIcon icon18;
        icon18.addFile(QStringLiteral(":/MvgSoft/piple"), QSize(), QIcon::Normal, QIcon::Off);
        menuPipeLine->setIcon(icon18);
        menuReconstruct = new QMenu(menuBar);
        menuReconstruct->setObjectName(QStringLiteral("menuReconstruct"));
        QIcon icon19;
        icon19.addFile(QStringLiteral(":/MvgSoft/reconstruct"), QSize(), QIcon::Normal, QIcon::Off);
        menuReconstruct->setIcon(icon19);
        MvgSoftClass->setMenuBar(menuBar);

        menuBar->addAction(menuOpen->menuAction());
        menuBar->addAction(menuTools->menuAction());
        menuBar->addAction(menuReconstruct->menuAction());
        menuOpen->addAction(actionFiles);
        menuTools->addAction(actionComputeFocal);
        menuTools->addAction(menuMatche->menuAction());
        menuTools->addAction(menuPipeLine->menuAction());
        menuTools->addAction(actionCMVS);
        menuTools->addAction(actionMesh);
        menuTools->addAction(actionColor);
        menuMatche->addAction(actionEMatch);
        menuMatche->addAction(actionFMatch);
        menuMatche->addAction(actionHMatch);
        menuPipeLine->addAction(actionCMPMVS);
        menuPipeLine->addAction(actionPMVS);
        menuPipeLine->addAction(actionMeshLab);
        menuReconstruct->addAction(actionIncremental);
        menuReconstruct->addAction(actionGlobal);
        menuReconstruct->addAction(actionPMVS2);
        menuReconstruct->addAction(actionLibViso);

        retranslateUi(MvgSoftClass);

        QMetaObject::connectSlotsByName(MvgSoftClass);
    } // setupUi

    void retranslateUi(QMainWindow *MvgSoftClass)
    {
        MvgSoftClass->setWindowTitle(QApplication::translate("MvgSoftClass", "MvgSoft", 0));
        actionFiles->setText(QApplication::translate("MvgSoftClass", "OpenFiles", 0));
        actionFiles->setShortcut(QApplication::translate("MvgSoftClass", "Shift+F", 0));
        actionComputeFocal->setText(QApplication::translate("MvgSoftClass", "ComputeFocal", 0));
        actionIncremental->setText(QApplication::translate("MvgSoftClass", "Incremental", 0));
        actionGlobal->setText(QApplication::translate("MvgSoftClass", "Global", 0));
        actionEMatch->setText(QApplication::translate("MvgSoftClass", "EMatch", 0));
        actionFMatch->setText(QApplication::translate("MvgSoftClass", "FMatch", 0));
        actionHMatch->setText(QApplication::translate("MvgSoftClass", "HMatch", 0));
        actionCMPMVS->setText(QApplication::translate("MvgSoftClass", "CMPMVS", 0));
        actionPMVS->setText(QApplication::translate("MvgSoftClass", "PMVS", 0));
        actionMeshLab->setText(QApplication::translate("MvgSoftClass", "MeshLab", 0));
        actionCMVS->setText(QApplication::translate("MvgSoftClass", "CMVS", 0));
        actionPMVS2->setText(QApplication::translate("MvgSoftClass", "PMVS2", 0));
        actionMesh->setText(QApplication::translate("MvgSoftClass", "Mesh", 0));
        actionLibViso->setText(QApplication::translate("MvgSoftClass", "LibViso", 0));
        actionColor->setText(QApplication::translate("MvgSoftClass", "Color", 0));
        menuOpen->setTitle(QApplication::translate("MvgSoftClass", "Open", 0));
        menuTools->setTitle(QApplication::translate("MvgSoftClass", "Tools", 0));
        menuMatche->setTitle(QApplication::translate("MvgSoftClass", "Matche", 0));
        menuPipeLine->setTitle(QApplication::translate("MvgSoftClass", "PipeLine", 0));
        menuReconstruct->setTitle(QApplication::translate("MvgSoftClass", "Reconstruct", 0));
    } // retranslateUi

};

namespace Ui {
    class MvgSoftClass: public Ui_MvgSoftClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MVGSOFT_H
