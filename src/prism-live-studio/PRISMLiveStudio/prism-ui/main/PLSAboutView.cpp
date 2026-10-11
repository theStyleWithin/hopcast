#include "PLSAboutView.hpp"
#include "ui_PLSAboutView.h"
#include "ui-config.h"
#include "pls-common-language.hpp"
#include "utils-api.h"
#include "frontend-api.h"
#include "prism-version.h"

PLSAboutView::PLSAboutView(QWidget *parent) : PLSDialogView(parent)
{
	ui = pls_new<Ui::PLSAboutView>();
	setResizeEnabled(false);
	pls_set_css(this, {"PLSAboutView"});
	setupUi(ui);
	setFixedSize(342, 360);
	initSize(342, 360);
	ui->versionLabel->setText(QString("%1.%2.%3 (Build %4)").arg(PRISM_VERSION_MAJOR).arg(PRISM_VERSION_MINOR).arg(PRISM_VERSION_PATCH).arg(PRISM_VERSION_BUILD));
	bool disabled = pls_is_output_actived();
	ui->checkUpdateButton->setDisabled(disabled);
	pls_uistep_v2_set_title(this, QStringLiteral("About"));
#ifdef HOPCAST_NO_NAVER
	/* HopCast: no PRISM logo or update check; GPL attribution instead of NAVER's copyright line. */
	setWindowTitle(QStringLiteral("About HopCast"));
	ui->logoLabel->setStyleSheet(QStringLiteral("image: none; color: #ffffff; font-size: 30px; font-weight: bold;"));
	ui->logoLabel->setAlignment(Qt::AlignCenter);
	ui->logoLabel->setText(QStringLiteral("HopCast"));
	ui->logoInfoLabel->setText(QString());
	ui->checkUpdateButton->setVisible(false);
	/* The credit goes under the version: the stylesheet pins the bottom label's height. */
	ui->rightLabel->setText(QString());
	ui->versionLabel->setWordWrap(true);
	ui->versionLabel->setAlignment(Qt::AlignCenter);
	ui->versionLabel->setStyleSheet(QStringLiteral("min-height: 0px; max-height: 200px; color: #bababa; font-size: 12px;"));
	ui->versionLabel->setText(ui->versionLabel->text() +
				  QStringLiteral("\nFree software under the GNU GPL v2, based on\nPRISM Live Studio (NAVER Corp.) and OBS Studio."));
#endif

#if defined(Q_OS_MACOS)
	ui->verticalSpacer_2->changeSize(0, 51, QSizePolicy::Fixed, QSizePolicy::Fixed);
	setWindowTitle(tr("Mac.Title.About"));
#endif
}

PLSAboutView::~PLSAboutView()
{
	pls_delete(ui);
}

void PLSAboutView::on_checkUpdateButton_clicked()
{
	this->accept();
}
