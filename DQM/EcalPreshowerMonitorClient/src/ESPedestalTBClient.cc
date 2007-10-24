#include "DQM/EcalPreshowerMonitorClient/interface/ESPedestalTBClient.h"

#include "FWCore/Framework/interface/Event.h"
#include "DataFormats/Common/interface/Handle.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "DQMServices/Core/interface/MonitorElementBaseT.h"
#include "DQM/EcalPreshowerMonitorClient/interface/ESDQMUtils.h"

#include "TStyle.h"
#include "TH2F.h"
#include "TPaveText.h"

ESPedestalTBClient::ESPedestalTBClient(const ParameterSet& ps) {
  
  writeHisto_ = ps.getUntrackedParameter<bool>("writeHisto", true);
  writeHTML_  = ps.getUntrackedParameter<bool>("writeHTML", true);
  dumpRate_   = ps.getUntrackedParameter<int>("dumpRate", 100);
  outputFileName_ = ps.getUntrackedParameter<string>("outputFileName", "ESPedestalTB");
  rootFolder_ = ps.getUntrackedParameter<string>("rootFolder", "");
  htmlDir_    = ps.getUntrackedParameter<string>("htmlDir","/preshower/DQM/TB");
  htmlName_   = ps.getUntrackedParameter<string>("htmlName","ESPedestalTB.html");  
  sta_        = ps.getUntrackedParameter<bool>("RunStandalone", false);
  gain_       = ps.getUntrackedParameter<int>("ESGain", 1); // 1 : low gain, 2 : high gain

  // QT criteria
  noisy_threshold_[0] = 5.;
  noisy_threshold_[1] = 10.;
  bad_threshold_[0] = 2.;
  bad_threshold_[1] = 4.2;

  count_ = 0;
  run_ = -1;
  init_ = false;

  fg = new TF1("fg", "gaus");
}

ESPedestalTBClient::~ESPedestalTBClient(){

  delete fg;

}

void ESPedestalTBClient::beginJob(const EventSetup& context){

  dbe_ = Service<DaqMonitorBEInterface>().operator->();

  if (dbe_) {
    dbe_->setVerbose(1);
    dbe_->setCurrentFolder("ES/QT/PedestalTB");
    dbe_->rmdir("ES/QT/PedestalTB");
  }

}

void ESPedestalTBClient::endJob(){
  
  Char_t runNum_s[50];
  sprintf(runNum_s, "%08d", run_);
  outputFile_ = htmlDir_+"/"+runNum_s+"/"+outputFileName_+"_"+runNum_s+".root";
  
  if (writeHTML_) {
    doQT();
    htmlOutput(run_, htmlDir_, htmlName_);
  }

  if (writeHisto_) dbe_->save(outputFile_);
  dbe_->rmdir("ES/QT/PedestalTB");  

  if ( init_ ) this->cleanup();
}

void ESPedestalTBClient::setup() {

  init_ = true;

  Char_t hist[200];
  
  if (dbe_) {
    dbe_->setCurrentFolder("ES/QT/PedestalTB");
    sprintf(hist, "ES QT PedestalTB Mean");
    meMean_ = dbe_->book1D(hist, hist, 5000, -0.5, 4999.5);
    sprintf(hist, "ES QT PedestalTB RMS");
    meRMS_ = dbe_->book1D(hist, hist, 20, -0.5, 19.5);
    sprintf(hist, "ES QT PedestalTB Fit Mean");
    meFitMean_ = dbe_->book1D(hist, hist, 5000, -0.5, 4999.5);
    sprintf(hist, "ES QT PedestalTB Fit RMS");
    meFitRMS_ = dbe_->book1D(hist, hist, 20, -0.5, 19.5);
    sprintf(hist, "ES PedestalTB Quality Plane 1");
    mePedCol_[0] = dbe_->book2D(hist, hist, 128, 0, 128, 4, 0, 4);
    sprintf(hist, "ES PedestalTB Quality Plane 2");
    mePedCol_[1] = dbe_->book2D(hist, hist, 4, 0, 4, 128, 0, 128);
    for (int i=0; i<2; ++i) {
      for (int j=0; j<4; ++j) {
	for (int k=0; k<4; ++k) {
	  sprintf(hist, "ES Pedestal Mean RMS Z 1 P %d Col %02d Row %02d", i+1, j+1, k+1);
	  mePedMeanRMS_[i][j][k] = dbe_->book1D(hist, hist, 32, 0, 32);
	  sprintf(hist, "ES Pedestal RMS Z 1 P %d Col %02d Row %02d", i+1, j+1, k+1);
	  mePedRMS_[i][j][k] = dbe_->book1D(hist, hist, 32, 0, 32);

	  sprintf(hist, "ES Pedestal Fit Mean RMS Z 1 P %d Col %02d Row %02d", i+1, j+1, k+1);
	  mePedFitMeanRMS_[i][j][k] = dbe_->book1D(hist, hist, 32, 0, 32);
	  sprintf(hist, "ES Pedestal Fit RMS Z 1 P %d Col %02d Row %02d", i+1, j+1, k+1);
	  mePedFitRMS_[i][j][k] = dbe_->book1D(hist, hist, 32, 0, 32);
	}
      }
    }
  }
  
}

void ESPedestalTBClient::cleanup() {

  if (sta_) return;

  if (dbe_) {
    dbe_->setCurrentFolder("ES/QT/PedestalTB");
    if (meMean_) dbe_->removeElement( meMean_->getName() );
    if (meRMS_) dbe_->removeElement( meRMS_->getName() );
    if (meFitMean_) dbe_->removeElement( meFitMean_->getName() );
    if (meFitRMS_) dbe_->removeElement( meFitRMS_->getName() );
    meMean_ = 0;
    meRMS_ = 0;
    meFitMean_ = 0;
    meFitRMS_ = 0;
    for (int i=0; i<2; i++) {

      if (mePedCol_[i]) dbe_->removeElement( mePedCol_[i]->getName() );
      mePedCol_[i] = 0;

      for (int j=0; j<4; j++) {
	for (int k=0; k<4; k++) {
	  if (mePedMeanRMS_[i][j][k]) dbe_->removeElement( mePedMeanRMS_[i][j][k]->getName() );
	  if (mePedRMS_[i][j][k]) dbe_->removeElement( mePedRMS_[i][j][k]->getName() );
	  if (mePedFitMeanRMS_[i][j][k]) dbe_->removeElement( mePedFitMeanRMS_[i][j][k]->getName() );
	  if (mePedFitRMS_[i][j][k]) dbe_->removeElement( mePedFitRMS_[i][j][k]->getName() );
	  mePedMeanRMS_[i][j][k] = 0;
	  mePedRMS_[i][j][k] = 0;
	  mePedFitMeanRMS_[i][j][k] = 0;
	  mePedFitRMS_[i][j][k] = 0;
	}
      }
    }
  }
  
  init_ = false;
}

void ESPedestalTBClient::analyze(const Event& e, const EventSetup& context){
  
  if ( ! init_ ) this->setup();

  int runNum = e.id().run();
  Char_t runNum_s[50];
      
  if (runNum != run_) { 

    if (run_ > 0) {

      sprintf(runNum_s, "%08d", run_);
      outputFile_ = htmlDir_+"/"+runNum_s+"/"+outputFileName_+"_"+runNum_s+".root";
      
      if (writeHTML_) {
	doQT();
	htmlOutput(run_, htmlDir_, htmlName_);
      }

      if (writeHisto_) dbe_->save(outputFile_);
    }

    run_ = runNum; 
    count_ = 0;

    sprintf(runNum_s, "%08d", run_);
    outputFile_ = htmlDir_+"/"+runNum_s+"/"+outputFileName_+"_"+runNum_s+".root";
  }

  count_++;

  if ((count_ % dumpRate_) == 0) {
    if (writeHTML_) {
      doQT();
      htmlOutput(runNum, htmlDir_, htmlName_);
    }
    if (writeHisto_) dbe_->save(outputFile_);
  }

}

void ESPedestalTBClient::doQT() {

  ESDQMUtils::resetME( meMean_ );
  ESDQMUtils::resetME( meRMS_ );
  ESDQMUtils::resetME( meFitMean_ );
  ESDQMUtils::resetME( meFitRMS_ );

  int val = 0;
  for (int i=0; i<2; ++i) {    
    for (int j=0; j<4; ++j) {
      for (int k=0; k<4; ++k) {
	for (int m=0; m<32; ++m) {
	  
	  MonitorElement * occME = dbe_->get(getMEName(i+1, j+1, k+1, m+1));
	  
	  if (occME) {
	    MonitorElementT<TNamed>* occ = dynamic_cast<MonitorElementT<TNamed>*>(occME);           
	    TH1F *hPedestal = dynamic_cast<TH1F*> (occ->operator->());      

	    if (hPedestal->GetMean()!=0) {
	      hPedestal->Fit("fg","Q");
	      hPedestal->Fit("fg","RQ","",fg->GetParameter(1)-2.*fg->GetParameter(2),fg->GetParameter(1)+2.*fg->GetParameter(2));

	      meMean_->Fill(hPedestal->GetMean());
	      meRMS_->Fill(hPedestal->GetRMS());
	      meFitMean_->Fill(fg->GetParameter(1));
	      meFitRMS_->Fill(fg->GetParameter(2));
	    }

	    if (fg->GetParameter(2)>noisy_threshold_[gain_-1] && hPedestal->GetMean()!=0) val = 7;         
	    else if (hPedestal->GetMean()==0) val = 5;
	    else if (fg->GetParameter(2)<bad_threshold_[gain_-1] && hPedestal->GetMean()!=0) val = 1;
	    else val = 4;
	    if (i==0) mePedCol_[i]->setBinContent(abs(m-32)+32*j, k+1, val);
	    if (i==1) mePedCol_[i]->setBinContent(j+1, k*32+m+1, val);

	    if (hPedestal->GetMean()!=0) {	    
	      mePedMeanRMS_[i][j][k]->setBinContent(m+1, hPedestal->GetMean());	   
	      mePedMeanRMS_[i][j][k]->setBinError(m+1, hPedestal->GetRMS());	   
	      mePedRMS_[i][j][k]->setBinContent(m+1, hPedestal->GetRMS());
	      mePedFitMeanRMS_[i][j][k]->setBinContent(m+1, fg->GetParameter(1));	   
	      mePedFitMeanRMS_[i][j][k]->setBinError(m+1, fg->GetParameter(2));	   
	      mePedFitRMS_[i][j][k]->setBinContent(m+1, fg->GetParameter(2));
	    }

	  } else {
	    if (i==0) mePedCol_[i]->setBinContent(abs(m-32)+32*j, k+1, 5);
	    if (i==1) mePedCol_[i]->setBinContent(j+1, k*32+m+1, 5);
	  }
	  
	}	
      }
    }
  }

}

string ESPedestalTBClient::getMEName(const int & plane, const int & col, const int & row, const int & strip) {
  
  Char_t hist[500];
  sprintf(hist,"%sES/ESPedestalTBTask/ES Pedestal Z 1 P %d Col %02d Row %02d Str %02d",rootFolder_.c_str(),plane,col,row,strip);

  return hist;
}

void ESPedestalTBClient::htmlOutput(int run, string htmlDir, string htmlName) {

  cout<<"Going to output ESPedestalTBClient html ..."<<endl;
  
  Char_t run_s[50];
  sprintf(run_s, "%08d", run); 
  htmlDir = htmlDir+"/"+run_s;
  system(("/bin/mkdir -m 777 -p " + htmlDir).c_str());

  ofstream htmlFile;   
  htmlFile.open((htmlDir+"/"+htmlName).c_str()); 

  // html page header
  htmlFile << "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">  " << endl;
  htmlFile << "<html>  " << endl;
  htmlFile << "<head>  " << endl;
  htmlFile << "  <meta content=\"text/html; charset=UTF-8\"  " << endl;
  htmlFile << " http-equiv=\"content-type\">  " << endl;
  htmlFile << "  <title>Preshower DQM : PedestalTBTask output</title> " << endl;
  htmlFile << "</head>  " << endl;
  htmlFile << "<style type=\"text/css\"> td { font-weight: bold } </style>" << endl;
  htmlFile << "<body>  " << endl;
  htmlFile << "<a name=""top""></a>" << endl;
  htmlFile << "<h2>Run Number / Num of Analyzed events :&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; <span " << endl;
  htmlFile << " style=\"color: rgb(0, 0, 153);\">" << run << "&nbsp;&nbsp;&nbsp;/&nbsp;&nbsp;&nbsp;"<< count_ <<"</span></h2>" << endl;
  htmlFile << "<h2>Monitoring task :&nbsp;&nbsp;&nbsp;&nbsp; <span " << endl;
  if (gain_==1)
    htmlFile << " style=\"color: rgb(0, 0, 153);\">Test Beam PEDESTAL (Low Gain) </span></h2> " << endl;
  if (gain_==2)
    htmlFile << " style=\"color: rgb(0, 0, 153);\">Test Beam PEDESTAL (High Gain) </span></h2> " << endl;
  htmlFile << "<hr>" << endl;
  htmlFile << "<table border=1><tr><td bgcolor=red>This strip is too noisy</td>" << endl;
  htmlFile << "<td bgcolor=blue>This strip has problems</td>" << endl;
  htmlFile << "<td bgcolor=lime>This strip has NO problems</td>" << endl;
  htmlFile << "<td bgcolor=yellow>This strip is missing</td>" << endl;
  htmlFile << "<tr><td> &#62; "<<noisy_threshold_[gain_-1]<<" ADC </td>"<<endl;
  htmlFile << "<td> &#60; "<<bad_threshold_[gain_-1]<<" ADC && &#60;Ped&#62; != 0 </td>"<<endl;
  htmlFile << "<td> &#60; "<<noisy_threshold_[gain_-1]<<" ADC && &#62; "<<bad_threshold_[gain_-1]<<" ADC </td>"<<endl; 
  htmlFile << "<td> &#60;Ped&#62; == 0 </td></tr>"<<endl;
  htmlFile << "</table>" << endl;
  htmlFile << "<br>" << endl;

  // make plots
  string histName;
  gROOT->SetStyle("Plain");
  gStyle->SetPalette(1, 0);
  gStyle->SetStatW(0.3);
  gStyle->SetStatH(0.3);
  gStyle->SetGridStyle(1);

  TCanvas *cPedQ1 = new TCanvas("cPedQ1", "cPedQ1", 300, 300);
  TCanvas *cPedQ2 = new TCanvas("cPedQ2", "cPedQ2", 300, 300);
  TCanvas *cPed   = new TCanvas("cPed",  "cPed",  600, 300);
  TCanvas *cPedF  = new TCanvas("cPedF", "cPedF", 600, 300);

  MonitorElementT<TNamed>* PedQ[2];
  TH2F* hPedQ[2];
  for (int i=0; i<2; ++i) {
    PedQ[i] = dynamic_cast<MonitorElementT<TNamed>*>(mePedCol_[i]);           
    hPedQ[i] = dynamic_cast<TH2F*> (PedQ[i]->operator->());  
  }

  TPaveText *t;
  TPaveText *t1;
  TPaveText *t2;
  TPaveText *t3;
  TPaveText *t4;

  gStyle->SetOptStat("");
  cPedQ1->cd();
  gPad->SetGridx();
  gPad->SetGridy();
  hPedQ[0]->GetXaxis()->SetNdivisions(-104);
  hPedQ[0]->GetYaxis()->SetNdivisions(-104);
  hPedQ[0]->GetXaxis()->SetLabelSize(0);
  hPedQ[0]->GetYaxis()->SetLabelSize(0);
  hPedQ[0]->SetMinimum(-0.00000001);
  hPedQ[0]->SetMaximum(7.0);
  hPedQ[0]->SetTitle("Plane 1");
  hPedQ[0]->Draw("col");
  gPad->Update();
  t = (TPaveText*) gPad->GetPrimitive("title");
  t->SetTextColor(4);
  t->SetTextSize(.1);
  t->SetBorderSize(0);
  t->SetX1NDC(0.00); t->SetX2NDC(1);
  t->SetY1NDC(0.93); t->SetY2NDC(1);
  t1 = new TPaveText(0.18, .04, 0.23, .09, "NDC");
  t1->SetBorderSize(0);
  t1->SetFillColor(0);
  t1->SetTextSize(0.06);
  t1->AddText("1");
  t1->Draw();
  t2 = new TPaveText(0.38, .04, 0.43, .09, "NDC");
  t2->SetBorderSize(0);
  t2->SetFillColor(0);
  t2->SetTextSize(0.06);
  t2->AddText("2");
  t2->Draw();
  t3 = new TPaveText(0.58, .04, 0.63, .09, "NDC");
  t3->SetBorderSize(0);
  t3->SetFillColor(0);
  t3->SetTextSize(0.06);
  t3->AddText("3");
  t3->Draw();
  t4 = new TPaveText(0.78, .04, 0.83, .09, "NDC");
  t4->SetBorderSize(0);
  t4->SetFillColor(0);
  t4->SetTextSize(0.06);
  t4->AddText("4");
  t4->Draw();
  t1 = new TPaveText(0.05, .785, 0.1, .835, "NDC");
  t1->SetBorderSize(0);
  t1->SetFillColor(0);
  t1->SetTextSize(0.06);
  t1->AddText("4");
  t1->Draw();
  t2 = new TPaveText(0.05, .585, 0.1, .635, "NDC");
  t2->SetBorderSize(0);
  t2->SetFillColor(0);
  t2->SetTextSize(0.06);
  t2->AddText("3");
  t2->Draw();
  t3 = new TPaveText(0.05, .385, 0.1, .435, "NDC");
  t3->SetBorderSize(0);
  t3->SetFillColor(0);
  t3->SetTextSize(0.06);
  t3->AddText("2");
  t3->Draw();
  t4 = new TPaveText(0.05, .185, 0.1, .235, "NDC");
  t4->SetBorderSize(0);
  t4->SetFillColor(0);
  t4->SetTextSize(0.06);
  t4->AddText("1");
  t4->Draw();

  histName = htmlDir+"/Pedestal_Quality_P1.png";
  cPedQ1->SaveAs(histName.c_str());  

  cPedQ2->cd();
  gPad->SetGridx();
  gPad->SetGridy();
  hPedQ[1]->GetXaxis()->SetNdivisions(-104);
  hPedQ[1]->GetYaxis()->SetNdivisions(-104);
  hPedQ[1]->GetXaxis()->SetLabelSize(0);
  hPedQ[1]->GetYaxis()->SetLabelSize(0);
  hPedQ[1]->SetMinimum(-0.00000001);
  hPedQ[1]->SetMaximum(7.0);
  hPedQ[1]->SetTitle("Plane 2");
  hPedQ[1]->Draw("col");
  gPad->Update();
  t = (TPaveText*) gPad->GetPrimitive("title");
  t->SetTextColor(4);
  t->SetTextSize(.1);
  t->SetBorderSize(0);
  t->SetX1NDC(0.00); t->SetX2NDC(1);
  t->SetY1NDC(0.93); t->SetY2NDC(1);
  t1 = new TPaveText(0.18, .04, 0.23, .09, "NDC");
  t1->SetBorderSize(0);
  t1->SetFillColor(0);
  t1->SetTextSize(0.06);
  t1->AddText("1");
  t1->Draw();
  t2 = new TPaveText(0.38, .04, 0.43, .09, "NDC");
  t2->SetBorderSize(0);
  t2->SetFillColor(0);
  t2->SetTextSize(0.06);
  t2->AddText("2");
  t2->Draw();
  t3 = new TPaveText(0.58, .04, 0.63, .09, "NDC");
  t3->SetBorderSize(0);
  t3->SetFillColor(0);
  t3->SetTextSize(0.06);
  t3->AddText("3");
  t3->Draw();
  t4 = new TPaveText(0.78, .04, 0.83, .09, "NDC");
  t4->SetBorderSize(0);
  t4->SetFillColor(0);
  t4->SetTextSize(0.06);
  t4->AddText("4");
  t4->Draw();
  t1 = new TPaveText(0.05, .785, 0.1, .835, "NDC");
  t1->SetBorderSize(0);
  t1->SetFillColor(0);
  t1->SetTextSize(0.06);
  t1->AddText("4");
  t1->Draw();
  t2 = new TPaveText(0.05, .585, 0.1, .635, "NDC");
  t2->SetBorderSize(0);
  t2->SetFillColor(0);
  t2->SetTextSize(0.06);
  t2->AddText("3");
  t2->Draw();
  t3 = new TPaveText(0.05, .385, 0.1, .435, "NDC");
  t3->SetBorderSize(0);
  t3->SetFillColor(0);
  t3->SetTextSize(0.06);
  t3->AddText("2");
  t3->Draw();
  t4 = new TPaveText(0.05, .185, 0.1, .235, "NDC");
  t4->SetBorderSize(0);
  t4->SetFillColor(0);
  t4->SetTextSize(0.06);
  t4->AddText("1");
  t4->Draw();

  histName = htmlDir+"/Pedestal_Quality_P2.png";
  cPedQ2->SaveAs(histName.c_str());  

  delete t;
  delete t1;
  delete t2;
  delete t3;
  delete t4;

  // Plot Mean and RMS
  MonitorElementT<TNamed>* Mean = dynamic_cast<MonitorElementT<TNamed>*>(meMean_);
  TH1F *hMean = dynamic_cast<TH1F*> (Mean->operator->());
  MonitorElementT<TNamed>* RMS  = dynamic_cast<MonitorElementT<TNamed>*>(meRMS_);
  TH1F *hRMS = dynamic_cast<TH1F*> (RMS->operator->());

  gStyle->SetOptStat(111110);
  cPed->Divide(2,1);
  cPed->cd(1);
  hMean->GetXaxis()->SetNdivisions(5);
  hMean->Draw();
  cPed->cd(2);
  hRMS->Draw();
  histName = htmlDir+"/Pedestal_Mean_RMS.png";
  cPed->SaveAs(histName.c_str());

  // Plot Fit Mean and RMS
  MonitorElementT<TNamed>* FitMean = dynamic_cast<MonitorElementT<TNamed>*>(meFitMean_);
  TH1F *hFitMean = dynamic_cast<TH1F*> (FitMean->operator->());
  MonitorElementT<TNamed>* FitRMS  = dynamic_cast<MonitorElementT<TNamed>*>(meFitRMS_);
  TH1F *hFitRMS = dynamic_cast<TH1F*> (FitRMS->operator->());

  gStyle->SetOptStat(111110);
  cPedF->Divide(2,1);
  cPedF->cd(1);
  hFitMean->GetXaxis()->SetNdivisions(5);
  hFitMean->Draw();
  cPedF->cd(2);
  hFitRMS->Draw();
  histName = htmlDir+"/Pedestal_Fit_Mean_RMS.png";
  cPedF->SaveAs(histName.c_str());

  // Show plots
  htmlFile << "<table border=\"0\" cellspacing=\"0\" " << endl;
  htmlFile << "cellpadding=\"10\" align=\"\"> " << endl;
  htmlFile << "<tr align=\"\">" << endl;
  htmlFile << "<td colspan=\"2\"><img src=\"Pedestal_Mean_RMS.png\"></img></td>" << endl;
  htmlFile << "<td colspan=\"2\"><img src=\"Pedestal_Quality_P1.png\"></img></td>" << endl;
  htmlFile << "</tr>" << endl;
  htmlFile << "</table>" << endl;
  htmlFile << "<table border=\"0\" cellspacing=\"0\" " << endl;
  htmlFile << "cellpadding=\"10\" align=\"\"> " << endl;
  htmlFile << "<tr align=\"center\">" << endl;
  htmlFile << "<td colspan=\"2\"><img src=\"Pedestal_Fit_Mean_RMS.png\"></img></td>" << endl;
  htmlFile << "<td colspan=\"2\"><img src=\"Pedestal_Quality_P2.png\"></img></td>" << endl;
  htmlFile << "</tr>" << endl;
  htmlFile << "</table>" << endl;

  // html page footer
  htmlFile << "</body> " << endl;
  htmlFile << "</html> " << endl;

  htmlFile.close();

}
