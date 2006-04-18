#ifndef TPedValues_h
#define TPedValues_h

/**
* \file TPedValues.h 
* \brief Transient container
* Store all the pedestal values depending on the gain and pedestal offset
* $Date:
* $Revision:
* \author P. Govoni (pietro.govoni@cernNOSPAM.ch)
*/

#include <vector>

#include "TFile.h"
#include "TSinglePedEntry.h"
#include "TPedResult.h"

class TPedValues
{
  public:
  
    //! ctor
    TPedValues (double RMSmax = 2 , int bestPedestal = 200) ;

    //! copy ctor
    TPedValues (const TPedValues & orig) ;

    //! dtor
    ~TPedValues () ;
  
    //! add a single value
    void insert (const int gainId, 
                 const int crystal, 
                 const int DAC, 
                 const int pedestal) ;
    
    //! calculate the offset values for all the containers
    TPedResult terminate (const int & DACstart = 0, const int & DACend = 256) const ;

    //! check whether the scan was complete in the range
    int checkEntries (const int & DACstart = 0, const int & DACend = 256) const ;
     
    //! create a plot of the DAC pedestal trend
    int makePlot (TFile & rootFile, const std::string & dirName) const ;
     
    //! create a plot of the DAC pedestal trend
    int makePlot (const std::string & rootFileName, const std::string & dirName) const ;
     
  private:
    
    TSinglePedEntry m_entries[3][1700][256] ;
    // 1,305,600 elements
    int m_bestPedestal ;
    double m_RMSmax ;

} ;


#endif
