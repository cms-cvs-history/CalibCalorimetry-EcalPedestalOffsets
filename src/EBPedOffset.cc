/**
 * \file EBPedOffset.cc
 *
 * $Date:  $
 * $Revision:  $
 * \author P. Govoni (pietro.govoni@cernNOSPAM.ch)
 * Last updated: @DATE@ @AUTHOR@
 *
*/

#include "DataFormats/EcalRecHit/interface/EcalUncalibratedRecHit.h"
#include "DataFormats/EcalRecHit/interface/EcalRecHitCollections.h"
#include "DataFormats/EcalRawData/interface/EcalDCCHeaderBlock.h"
#include "DataFormats/EcalRawData/interface/EcalRawDataCollections.h"
#include <DataFormats/EcalDigi/interface/EcalDigiCollections.h>
#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "OnlineDB/EcalCondDB/interface/EcalCondDBInterface.h"
#include "OnlineDB/EcalCondDB/interface/all_monitoring_types.h"

#include "CalibCalorimetry/EcalPedestalOffsets/interface/EBPedOffset.h"
#include <fstream>
#include <iostream>

//! ctor
EBPedOffset::EBPedOffset (const ParameterSet& paramSet) :
  m_digiCollection (paramSet.getParameter<std::string> ("digiCollection")) ,
  m_digiProducer (paramSet.getParameter<std::string> ("digiProducer")) ,
  m_headerProducer (paramSet.getParameter<std::string> ("headerProducer")) ,
  m_xmlFile (paramSet.getParameter<std::string> ("xmlFile")) ,
  m_DACmin (paramSet.getParameter<int> ("DACmin")) ,
  m_DACmax (paramSet.getParameter<int> ("DACmax")) ,
  m_RMSmax (paramSet.getParameter<double> ("RMSmax")) ,
  m_bestPed (paramSet.getParameter<int> ("bestPed")) , 
  m_SMnum (paramSet.getParameter<int> ("SMnum")) ,
  m_dbHostName (paramSet.getParameter<std::string> ("dbHostName")) ,
  m_dbName (paramSet.getParameter<std::string> ("dbName")) ,
  m_dbUserName (paramSet.getParameter<std::string> ("dbUserName")) ,
  m_dbPassword (paramSet.getParameter<std::string> ("dbPassword")) ,
  m_run (paramSet.getParameter<int> ("run"))    
{
  std::cout << "[EBPedOffSet][ctor] reading "
            << "\n[EBPedOffSet][ctor] m_DACmin: " << m_DACmin
            << "\n[EBPedOffSet][ctor] m_DACmax: " << m_DACmax
            << "\n[EBPedOffSet][ctor] m_RMSmax: " << m_RMSmax
            << "\n[EBPedOffSet][ctor] m_bestPed: " << m_bestPed
            << std::endl ;
}


//! dtor
EBPedOffset::~EBPedOffset ()
{
  for (std::map<int,TPedValues*>::iterator mapIt = m_pedValues.begin () ;
       mapIt != m_pedValues.end () ;
       ++mapIt)
    delete mapIt->second ; 
  for (std::map<int,TPedResult*>::iterator mapIt = m_pedResult.begin () ;
       mapIt != m_pedResult.end () ;
       ++mapIt)
    delete mapIt->second ; 
}


//! begin the job
void EBPedOffset::beginJob (EventSetup const& eventSetup)
{
   std::cout << "[EBPedOffset][beginJob] " << std::endl ;
}


//! perform te analysis
void EBPedOffset::analyze (Event const& event, 
                           EventSetup const& eventSetup) 
{
   std::cout << "[EBPedOffset][analyze] "
    << std::endl;

   // get the headers
   // (one header for each supermodule)
   edm::Handle<EcalRawDataCollection> DCCHeaders ;
   try {
     event.getByLabel (m_headerProducer, DCCHeaders) ;
   } catch ( std::exception& ex ) {
     std::cerr << "Error! can't get the product " << m_headerProducer.c_str () 
               << std::endl ;
   }

   std::map <int,int> DACvalues ;
   
   // loop over the headers
   for ( EcalRawDataCollection::const_iterator headerItr= DCCHeaders->begin () ;
         headerItr != DCCHeaders->end () ; 
	     ++headerItr ) 
     {
       EcalDCCEventSettings settings = headerItr->getEventSettings () ;
       DACvalues[getHeaderSMId (headerItr->id ())] = settings.ped_offset ;
//       std::cout << "DCCid: " << headerItr->id () << "\n" ;
//       std::cout << "Ped offset DAC: " << settings.ped_offset << "\n" ;
     } //! loop over the headers

   // get the digis
   // (one digi for each crystal)
   Handle<EBDigiCollection> pDigis;
   try {
     event.getByLabel (m_digiProducer, pDigis) ;
   } catch ( std::exception& ex ) 
   {
     std::cerr << "Error! can't get the product " << m_digiCollection.c_str () 
               << std::endl ;
   }
   
   // loop over the digis
   for (EBDigiCollection::const_iterator itdigi = pDigis->begin () ; 
        itdigi != pDigis->end () ; 
        ++itdigi) 
    {    
       int gainId = itdigi->sample (0).gainId () ;
       int crystalId = itdigi->id ().ic () ;
//       int crystalId = itdigi->id().iphi () + 20 * (itdigi->id().ieta () -1) ;
       int smId = itdigi->id ().ism () ;
       if (!m_pedValues.count (smId)) m_pedValues[smId] = new TPedValues (m_RMSmax,m_bestPed) ;

       // loop over the samples
       for (int iSample = 0; iSample < EBDataFrame::MAXSAMPLES ; ++iSample) 
         {
            m_pedValues[smId]->insert (gainId,
                                       crystalId,
                                       DACvalues[smId],
                                       itdigi->sample (iSample).adc ()) ;
/* FIXME
            printf ("[pietro][input] %d %d %d %d\n", gainId,
                                       crystalId,
                                       DACvalues[smId],
                                       itdigi->sample (iSample).adc ()) ;
*/
         } // loop over the samples
    } // loop over the digis

}


//! perform the minimiation and write results
void EBPedOffset::endJob () 
{
  for (std::map<int,TPedValues*>::const_iterator smPeds = m_pedValues.begin () ;
       smPeds != m_pedValues.end () ; 
       ++smPeds)
    {
      m_pedResult[smPeds->first] = 
        new TPedResult ((smPeds->second)->terminate (m_DACmin, m_DACmax)) ;
    } 
  std::cout << "[EBPedOffset][endJob] results map size " 
            << m_pedResult.size ()
            << std::endl ;
  if (m_dbHostName != "0") writeDb () ;          
  writeXMLFile (m_xmlFile) ;
}


//! write the m_pedResult in the DB
//!FIXME divide into sub-tasks
void EBPedOffset::writeDb () 
{
  // connect to the database
  EcalCondDBInterface* DBconnection ;
  try {
    DBconnection = new EcalCondDBInterface (m_dbHostName, m_dbName, 
                                            m_dbUserName, m_dbPassword) ; 
  } catch (runtime_error &e) {
    cerr << e.what() << endl ;
    return ;
  }

  // define the query to get the right place in the database
  RunTag runtag ;  
  LocationDef locdef ;
  RunTypeDef rundef ;
  locdef.setLocation ("H4") ;
  rundef.setRunType ("PEDESTAL-OFFSET") ;
  rundef.setConfigTag ("PEDESTAL-OFFSET_SCAN") ;
  rundef.setConfigVersion (1) ;  // for H4
  runtag.setLocationDef (locdef) ;
  runtag.setRunTypeDef (rundef) ;
  runtag.setGeneralTag ("PEDESTAL-OFFSET") ;
  run_t run = m_run ; //FIXME dal config file
  RunIOV runiov = DBconnection->fetchRunIOV (&runtag, run) ;

  MonVersionDef monverdef ;  
  monverdef.setMonitoringVersion ("test01") ;
  MonRunTag montag ;
  montag.setMonVersionDef (monverdef) ;
  montag.setGeneralTag ("CMSSW") ;

  subrun_t subrun = 1 ; //hardcoded!
  
  // get the interval of validity where to ins
  MonRunIOV moniov = DBconnection->fetchMonRunIOV (&runtag, &montag, run, subrun) ;

  // create the table to be filled and the map to be inserted
  EcalLogicID ecid ;
  map<EcalLogicID, MonPedestalOffsetsDat> DBdataset ;
  MonPedestalOffsetsDat DBtable ;

  // fill the table

  // loop over the super-modules
  for (std::map<int,TPedResult*>::const_iterator result = m_pedResult.begin () ;
       result != m_pedResult.end () ;
       ++result)
    {
      // loop over the crystals
      for (int xtal = 0 ; xtal<1700 ; ++xtal)
        {
          DBtable.setDACG1 (result->second->m_DACvalue[2][xtal]) ;
          DBtable.setDACG6 (result->second->m_DACvalue[1][xtal]) ;
          DBtable.setDACG12 (result->second->m_DACvalue[0][xtal]) ;
          DBtable.setTaskStatus (1) ; //FIXME to be set correctly

          // fill the table
          if ( DBconnection ) 
            {
              try {
                ecid = DBconnection->getEcalLogicID ("EB_crystal_number", 
                                                     result->first, xtal) ;
                DBdataset[ecid] = DBtable ;
              } catch (runtime_error &e) {
                cerr << e.what() << endl ;
              }
           }
        } // loop over the crystals
    } // loop over the super-modules

  // insert the map of tables in the database
  if ( DBconnection ) {
    try {
      cout << "Inserting dataset ... " << flush;
      if ( DBdataset.size() != 0 ) DBconnection->insertDataSet (&DBdataset, &moniov) ;
      cout << "done." << endl ;
    } catch (runtime_error &e) {
      cerr << e.what () << endl ;
    }
  }

}


//!calculate the super module number from the headers
int EBPedOffset::getHeaderSMId (const int headerId) 
{
  //PG FIXME temporary solution
  //PG FIXME check it is consistent with the TB!
  return 1 ;
  /*
  unsigned dccID = 1-1 ; // at the moment SM is 1 by default (in DetID)
  EBDetId idsm (1, 1 + 20 * dccID) ;  
  // da qui poi, se sono eta e phi, si deduce smid (da EBDetId):
  int id = ( iphi() - 1 ) / kCrystalsInPhi + 1;
  if ( zside() < 0 ) id += 18;
  */
}


//! write the m_pedResult in an XML file
void EBPedOffset::writeXMLFile (std::string fileName) 
{
  // open the output stream
  std::ofstream xml_outfile ;
  xml_outfile.open (fileName.c_str ()) ;
  
  // write the header file
  // FIXME has the SM number to be removed form here?
  xml_outfile<<"<offsets>"<<std::endl;
  xml_outfile << "<PEDESTAL_OFFSET_RELEASE VERSION_ID = \"SM1_VER1\"> \n" ;
  xml_outfile << "  <RELEASE_ID>RELEASE_1</RELEASE_ID>\n" ;
  xml_outfile << "  <SUPERMODULE> " << "-1" << " </SUPERMODULE>\n" ;
  xml_outfile << "  <TIME_STAMP> 070705 </TIME_STAMP>" << std::endl ;

  // loop over the super-modules
  for (std::map<int,TPedResult*>::const_iterator smRes = m_pedResult.begin () ;
       smRes != m_pedResult.end () ; 
       ++smRes)
    {
       // loop over the crystals
       for (int xtal = 0 ; xtal < 1700 ; ++xtal) 
         {
            xml_outfile << "  <PEDESTAL_OFFSET>\n";
            xml_outfile << "    <HIGH>" << ((smRes->second)->m_DACvalue)[0][xtal] << "</HIGH>\n" ;
            xml_outfile << "    <MED>" << ((smRes->second)->m_DACvalue)[1][xtal] << "</MED>\n" ;
            xml_outfile << "    <LOW>" << ((smRes->second)->m_DACvalue)[2][xtal] << "</LOW>\n" ;
            xml_outfile << "    <SUPERMODULE> " << m_SMnum << " </SUPERMODULE>\n" ;
// FIXME the right version below, waiting for the fix in CMSSW
//             xml_outfile << "    <SUPERMODULE> " << smRes->first << " </SUPERMODULE>\n" ;
            xml_outfile << "    <CRYSTAL> "<< xtal+1 << " </CRYSTAL>\n" ;
            xml_outfile << "  </PEDESTAL_OFFSET>" << std::endl ;            
         } // loop over the crystals
    } // loop over the super-modules
  
  // close the open tags  
  xml_outfile << " </PEDESTAL_OFFSET_RELEASE>" << std::endl ;
  xml_outfile << "</offsets>" << std::endl ;
  xml_outfile.close () ;

}


void EBPedOffset::subscribe ()
{}

void EBPedOffset::subscribeNew ()
{}

void EBPedOffset::unsubscribe ()
{}


/* various pieces of code here 
// ----------------------------   


       // loop over the samples
       for (int iSample = 0; iSample < EBDataFrame::MAXSAMPLES ; iSample++) 
           {
              if (itdigi->sample (iSample).gainId () != gainId) 
                {
                  average = 0 ;
                  break ; 
                  //PG do domething there is an error!
                }
              average += double (itdigi->sample (iSample).adc ()) ;
            } // loop over the samples
       if (!average) 
         {
           continue ; 
           //PG do domething there is an error!
         }
       average /= EBDataFrame::MAXSAMPLES ;
       int smId = itdigi->id ().ism () ;

       if (!m_pedValues.count (smId)) m_pedValues[smId] = new TPedValues () ;
       m_pedValues[smId]->insert (gainId,
                                 itdigi->id ().ic (),
                                 DACvalues[smId],
                                 static_cast<int> (average)) ;







*/
