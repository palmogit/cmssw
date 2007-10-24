#include "DQM/EcalPreshowerMonitorModule/interface/ESPedestalTBTask.h"
#include "TBDataFormats/HcalTBObjects/interface/HcalTBTriggerData.h"

#include <iostream>

using namespace cms;
using namespace edm;
using namespace std;

ESPedestalTBTask::ESPedestalTBTask(const ParameterSet& ps) {

  label_        = ps.getUntrackedParameter<string>("Label");
  instanceName_ = ps.getUntrackedParameter<string>("InstanceES");
  sta_          = ps.getUntrackedParameter<bool>("RunStandalone", false);

  init_ = false;

  for (int i=0; i<2; ++i) 
    for (int j=0; j<4; ++j)
      for (int k=0; k<4; ++k)        
	for (int m=0; m<32; ++m)
	  mePedestal_[i][j][k][m] = 0;  

}

ESPedestalTBTask::~ESPedestalTBTask(){
}

void ESPedestalTBTask::beginJob(const EventSetup& c) {
  
  ievt_ = 0;
  
  dbe_ = Service<DaqMonitorBEInterface>().operator->();
  
  if ( dbe_ ) {
    dbe_->setCurrentFolder("ES/ESPedestalTBTask");
    dbe_->rmdir("ES/ESPedestalTBTask");
  }
  
}

void ESPedestalTBTask::setup(void){
  
  init_ = true;
  
  Char_t hist[300];
  
  dbe_ = Service<DaqMonitorBEInterface>().operator->();
  
  if ( dbe_ ) {   
    dbe_->setCurrentFolder("ES/ESPedestalTBTask");
    for (int i=0; i<2; ++i) {       
      for (int j=0; j<4; ++j) {
	for (int k=0; k<4; ++k) {
	  for (int m=0; m<32; ++m) {
	    sprintf(hist, "ES Pedestal Z 1 P %d Col %02d Row %02d Str %02d", i+1, j+1, k+1, m+1);      
	    mePedestal_[i][j][k][m] = dbe_->book1D(hist, hist, 5000, -0.5, 4999.5);
	  }
	}
      }
    }
  }
  
}

void ESPedestalTBTask::cleanup(void){

  dbe_ = Service<DaqMonitorBEInterface>().operator->();

  if ( dbe_ ) {
    dbe_->setCurrentFolder("ES/ESPedestalTBTask");

    for (int i=0; i<2; ++i) {
      for (int j=0; j<4; ++j) {
	for (int k=0; k<4; ++k) {
	  for (int m=0; m<32; ++m) {
	      if ( mePedestal_[i][j][k][m] ) dbe_->removeElement( mePedestal_[i][j][k][m]->getName() );
	      mePedestal_[i][j][k][m] = 0;
	    }
	  }
	}
      }
  }  

  init_ = false;

}

void ESPedestalTBTask::endJob(void) {

  LogInfo("ESPedestalTBTask") << "analyzed " << ievt_ << " events";

  if (sta_) return;

  if ( init_ ) this->cleanup();

}

void ESPedestalTBTask::analyze(const Event& e, const EventSetup& c){

  if ( ! init_ ) this->setup();
  ievt_++;
  
  Handle<HcalTBTriggerData> trg;
  try {
    e.getByType(trg);
  }
  catch ( cms::Exception &e ) {
    LogDebug("") << "ESPedestal : Error! can't get trigger information !" << std::endl;
  }

  int trgbit = 0;
  if( trg->wasBeamTrigger() )             trgbit = 1;
  if( trg->wasInSpillPedestalTrigger() )  trgbit = 2;
  if( trg->wasOutSpillPedestalTrigger() ) trgbit = 3;
  if( trg->wasLEDTrigger() )              trgbit = 4;
  if( trg->wasLaserTrigger() )            trgbit = 5;

  Handle<ESDigiCollection> digis;
  try {
    e.getByLabel(label_, instanceName_, digis);
  } catch ( cms::Exception &e ) {
    LogDebug("") << "ESPedestal : Error! can't get collection !" << std::endl;
  } 

  if (trgbit == 1 || trgbit == 4 || trgbit == 5) return;

  for (ESDigiCollection::const_iterator digiItr = digis->begin(); digiItr != digis->end(); ++digiItr ) {

    ESDataFrame dataframe = (*digiItr);
    ESDetId id = dataframe.id();

    int plane = id.plane();
    int ix    = id.six();
    int iy    = id.siy();
    int strip = id.strip();

    for (int isam=0; isam<dataframe.size(); ++isam) {
      mePedestal_[plane-1][ix-1][iy-1][strip-1]->Fill(dataframe.sample(isam).adc());
    }
    
  }

}
