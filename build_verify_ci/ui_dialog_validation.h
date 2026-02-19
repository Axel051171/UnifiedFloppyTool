/********************************************************************************
** Form generated from reading UI file 'dialog_validation.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DIALOG_VALIDATION_H
#define UI_DIALOG_VALIDATION_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_DialogValidation
{
public:
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QLabel *labelIcon;
    QLabel *labelTitle;
    QGroupBox *groupError;
    QVBoxLayout *verticalLayout_2;
    QLabel *labelErrorMessage;
    QTextEdit *textErrorDetails;
    QGroupBox *groupSuggestion;
    QVBoxLayout *verticalLayout_3;
    QLabel *labelSuggestion;
    QCheckBox *checkDontShowAgain;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *DialogValidation)
    {
        if (DialogValidation->objectName().isEmpty())
            DialogValidation->setObjectName("DialogValidation");
        DialogValidation->resize(500, 350);
        DialogValidation->setModal(true);
        verticalLayout = new QVBoxLayout(DialogValidation);
        verticalLayout->setObjectName("verticalLayout");
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        labelIcon = new QLabel(DialogValidation);
        labelIcon->setObjectName("labelIcon");
        labelIcon->setMinimumSize(QSize(64, 64));
        labelIcon->setMaximumSize(QSize(64, 64));
        labelIcon->setAlignment(Qt::AlignCenter);
        labelIcon->setStyleSheet(QString::fromUtf8("font-size: 48pt;"));

        horizontalLayout->addWidget(labelIcon);

        labelTitle = new QLabel(DialogValidation);
        labelTitle->setObjectName("labelTitle");
        labelTitle->setStyleSheet(QString::fromUtf8("font-size: 16pt; font-weight: bold;"));
        labelTitle->setWordWrap(true);

        horizontalLayout->addWidget(labelTitle);


        verticalLayout->addLayout(horizontalLayout);

        groupError = new QGroupBox(DialogValidation);
        groupError->setObjectName("groupError");
        verticalLayout_2 = new QVBoxLayout(groupError);
        verticalLayout_2->setObjectName("verticalLayout_2");
        labelErrorMessage = new QLabel(groupError);
        labelErrorMessage->setObjectName("labelErrorMessage");
        labelErrorMessage->setWordWrap(true);
        labelErrorMessage->setStyleSheet(QString::fromUtf8("font-size: 11pt;"));

        verticalLayout_2->addWidget(labelErrorMessage);

        textErrorDetails = new QTextEdit(groupError);
        textErrorDetails->setObjectName("textErrorDetails");
        textErrorDetails->setMaximumSize(QSize(16777215, 100));
        textErrorDetails->setReadOnly(true);

        verticalLayout_2->addWidget(textErrorDetails);


        verticalLayout->addWidget(groupError);

        groupSuggestion = new QGroupBox(DialogValidation);
        groupSuggestion->setObjectName("groupSuggestion");
        verticalLayout_3 = new QVBoxLayout(groupSuggestion);
        verticalLayout_3->setObjectName("verticalLayout_3");
        labelSuggestion = new QLabel(groupSuggestion);
        labelSuggestion->setObjectName("labelSuggestion");
        labelSuggestion->setWordWrap(true);
        labelSuggestion->setStyleSheet(QString::fromUtf8("color: rgb(0, 128, 0);"));

        verticalLayout_3->addWidget(labelSuggestion);


        verticalLayout->addWidget(groupSuggestion);

        checkDontShowAgain = new QCheckBox(DialogValidation);
        checkDontShowAgain->setObjectName("checkDontShowAgain");

        verticalLayout->addWidget(checkDontShowAgain);

        buttonBox = new QDialogButtonBox(DialogValidation);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

        verticalLayout->addWidget(buttonBox);


        retranslateUi(DialogValidation);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, DialogValidation, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, DialogValidation, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(DialogValidation);
    } // setupUi

    void retranslateUi(QDialog *DialogValidation)
    {
        DialogValidation->setWindowTitle(QCoreApplication::translate("DialogValidation", "Validation Error", nullptr));
        labelIcon->setText(QCoreApplication::translate("DialogValidation", "\342\232\240\357\270\217", nullptr));
        labelTitle->setText(QCoreApplication::translate("DialogValidation", "Configuration Error", nullptr));
        groupError->setTitle(QCoreApplication::translate("DialogValidation", "Error Details", nullptr));
        labelErrorMessage->setText(QCoreApplication::translate("DialogValidation", "The selected configuration is invalid.", nullptr));
        textErrorDetails->setHtml(QCoreApplication::translate("DialogValidation", "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n"
"<html><head><meta name=\"qrichtext\" content=\"1\" /></head><body>\n"
"<p><span style=\" font-weight:600;\">Issues found:</span></p>\n"
"<ul>\n"
"<li>Tracks set to 90 (maximum is 83)</li>\n"
"<li>Format mismatch: C64 GCR selected but MFM encoding active</li>\n"
"</ul>\n"
"</body></html>", nullptr));
        groupSuggestion->setTitle(QCoreApplication::translate("DialogValidation", "Suggested Fix", nullptr));
        labelSuggestion->setText(QCoreApplication::translate("DialogValidation", "\342\200\242 Set Tracks to 83 (maximum supported)\n"
"\342\200\242 Change encoding to GCR for C64 format", nullptr));
        checkDontShowAgain->setText(QCoreApplication::translate("DialogValidation", "Don't show this warning again (not recommended)", nullptr));
    } // retranslateUi

};

namespace Ui {
    class DialogValidation: public Ui_DialogValidation {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_DIALOG_VALIDATION_H
