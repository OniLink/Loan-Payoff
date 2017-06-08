#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <qwt_plot_curve.h>
#include <vector>

namespace Ui {
class MainWindow;
}

enum PayMethod {
	principleFirst,
	interestFirst
};

struct LoanData {
	double principle;
	double mpr;
	unsigned int period;
	double minimum;
};

class MainWindow : public QMainWindow {
		Q_OBJECT

	public:
		explicit MainWindow( QWidget* parent = nullptr );
		~MainWindow();

	public slots:
		void addLoan();
		void deleteLoan();
		void selectMethodPrinciple();
		void selectMethodInterest();
		void refreshPlot( double payment );

	private:
		Ui::MainWindow* ui;
		PayMethod method;
		std::vector< LoanData > loans;
		std::vector< QwtPlotCurve* > plots;

		unsigned int pickPriority( QVector< unsigned int > choices );
};

#endif // MAINWINDOW_HPP
