#include <QStringList>
#include <cmath>
#include "MainWindow.hpp"
#include "ui_MainWindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow) {
	ui->setupUi(this);

	QStringList headers;
	headers << "Loan Name" << "Principle" << "Interest (APR)" << "Period (Months)" << "Minimum Payment";
	ui->tableLoans->setHorizontalHeaderLabels( headers );

	ui->plotLoans->setTitle( "Loan Progress" );
	ui->plotLoans->setAxisTitle( QwtPlot::Axis::xBottom, "Time (Months)" );
	ui->plotLoans->setAxisTitle( QwtPlot::Axis::yLeft, "Value ($)" );

	if( ui->entryMethodPrinciple->isChecked() ) {
		method = PayMethod::principleFirst;
	} else if( ui->entryMethodInterest->isChecked() ) {
		method = PayMethod::interestFirst;
	} else {
		method = PayMethod::principleFirst;
		ui->entryMethodPrinciple->setChecked( true );
	}
}

double calculateMinimumPayment( double principle, double mpr, unsigned int period ) {
	// Calculation below derived from dP/dt = rP - I, P(0) = Principle, P(period) = 0
	// Rounded up to the next cent
	double minimum_factor = mpr / ( 1. - std::exp( -mpr * static_cast< double >( period ) ) );
	double minimum = minimum_factor * principle;
	return std::ceil( 100. * minimum ) / 100.;
}

double calculatePayoffTime( double principle, double mpr, double payment ) {
	// Derived from dP/dt = rP - I, P(0) = Principle, P(payoff) = 0
	return std::log( payment / ( payment - mpr * principle ) ) / mpr;
}

double updatePrinciple( double principle, double mpr, double payment, double time ) {
	// Derived from dP/dt = rP - I, P(0) = Principle
	double exponential = std::exp( mpr * time );
	double next = principle * exponential + payment / mpr * ( 1. - exponential );
	if( next < 0.01 ) {
		return 0.;
	}
	return next;
}

void MainWindow::addLoan() {
	LoanData loan;
	loan.principle = ui->entryLoanPrinciple->value();
	loan.mpr = ui->entryLoanAPR->value() / 1200.; // Convert APR (1 year, 100=100%) to MPR (1 month, 1=100%)
	loan.period = ui->entryLoanPeriod->value();
	loan.minimum = calculateMinimumPayment( loan.principle, loan.mpr, loan.period );

	int row = ui->tableLoans->rowCount();
	loans.push_back( loan );
	ui->tableLoans->insertRow( row );
	ui->tableLoans->setCellWidget( row, 0, new QLabel( ui->entryLoanName->text() ) );
	ui->tableLoans->setCellWidget( row, 1, new QLabel( QString::number( loan.principle, 'f', 2 ) ) );
	ui->tableLoans->setCellWidget( row, 2, new QLabel( QString::number( loan.mpr * 1200., 'f', 2 ) ) );
	ui->tableLoans->setCellWidget( row, 3, new QLabel( QString::number( loan.period ) ) );
	ui->tableLoans->setCellWidget( row, 4, new QLabel( QString::number( loan.minimum, 'f', 2 ) ) );

	QwtPlotCurve* new_plot = new QwtPlotCurve( ui->entryLoanName->text() );
	new_plot->attach( ui->plotLoans );
	plots.push_back( new_plot );

	refreshPlot( ui->entryMonthlyPayment->value() );
}

void MainWindow::deleteLoan() {
	int row = ui->tableLoans->currentRow();
	if( row < 0 ) {
		return;
	}

	ui->tableLoans->removeRow( row );
	loans.erase( loans.begin() + row );
	plots.at( row )->detach();
	delete plots.at( row );
	plots.erase( plots.begin() + row );

	refreshPlot( ui->entryMonthlyPayment->value() );
}

void MainWindow::selectMethodPrinciple() {
	method = PayMethod::principleFirst;
	refreshPlot( ui->entryMonthlyPayment->value() );
}

void MainWindow::selectMethodInterest() {
	method = PayMethod::interestFirst;
	refreshPlot( ui->entryMonthlyPayment->value() );
}

void MainWindow::refreshPlot( double payment ) {
	double total_minimum = 0.;
	for( auto& loan_data : loans ) {
		total_minimum += loan_data.minimum;
	}

	if( payment < total_minimum ) {
		payment = total_minimum;
		ui->entryMonthlyPayment->setValue( payment );
	}

	QVector< double > month_data;
	QVector< QVector< double > > money_data;
	QVector< unsigned int > unpaid;

	for( unsigned int i = 0; i < loans.size(); ++i ) {
		unpaid.push_back( i );
	}

	month_data.push_back( 0. );
	for( unsigned int i = 0; i < loans.size(); ++i ) {
		QVector< double > new_money;
		new_money.push_back( loans[ i ].principle );
		money_data.push_back( new_money );
	}

	while( unpaid.size() > 0 ) {
		unsigned int priority = pickPriority( unpaid );

		// Find out how much extra money we have
		double extra = payment;
		for( auto i : unpaid ) {
			extra -= loans[ i ].minimum;
		}

		// Find out if we're paying any loans off soon...
		double next_payoff = 2.;
		for( auto i : unpaid ) {
			double current_pay = loans[ i ].minimum;
			if( i == priority ) {
				current_pay += extra;
			}

			double payoff = calculatePayoffTime( money_data[ i ].back(), loans[ i ].mpr, current_pay );
			if( payoff < next_payoff ) {
				next_payoff = payoff + 0.0001; // add a small extra step to get over an infinite loop
			}
		}

		// Pick our time step (either next payoff or 1 month)
		double step = 1.;
		if( next_payoff < 1. ) {
			step = next_payoff;
		}

		// Update data
		month_data.push_back( month_data.back() + step );
		for( auto i : unpaid ) {
			double current_pay = loans[ i ].minimum;
			if( i == priority ) {
				current_pay += extra;
			}

			double next_money = updatePrinciple( money_data[ i ].back(), loans[ i ].mpr, current_pay, step );
			money_data[ i ].push_back( next_money );
		}

		// Remove paid loans
		for( int i = 0; i < unpaid.size(); ++i ) {
			if( money_data[ unpaid[ i ] ].back() < 0.01 ) {
				unpaid.remove( i );
			}
		}
	}

	// Pad out any short money lists
	for( unsigned int i = 0; i < loans.size(); ++i ) {
		while( money_data[ i ].size() < month_data.size() ) {
			money_data[ i ].push_back( 0. );
		}
	}

	// Update data
	for( unsigned int i = 0; i < loans.size(); ++i ) {
		plots[ i ]->setSamples( month_data, money_data[ i ] );
	}

	ui->plotLoans->updateAxes();
	ui->plotLoans->replot();
}

unsigned int MainWindow::pickPriority( QVector< unsigned int > choices ) {
	if( choices.size() == 0 ) {
		return 0;
	}

	unsigned int choice = choices[ 0 ];

	if( method == PayMethod::principleFirst ) {
		double principle = loans[ choices[ 0 ] ].principle;

		for( int i = 1; i < choices.size(); ++i ) {
			if( loans[ choices[ i ] ].principle < principle ) {
				principle = loans[ choices[ i ] ].principle;
				choice = choices[ i ];
			}
		}
	} else if( method == PayMethod::interestFirst ) {
		double rate = loans[ choices[ 0 ] ].mpr;

		for( int i = 1; i < choices.size(); ++i ) {
			if( loans[ choices[ i ] ].mpr > rate ) {
				rate = loans[ choices[ i ] ].mpr;
				choice = choices[ i ];
			}
		}
	}

	return choice;
}

MainWindow::~MainWindow() {
	delete ui;
}
