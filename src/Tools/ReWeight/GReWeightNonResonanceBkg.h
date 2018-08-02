//____________________________________________________________________________
/*!

\class    genie::rew::GReWeightNonResonanceBkg

\brief    Reweighting non-resonance background level.

\author   Costas Andreopoulos <costas.andreopoulos \at stfc.ac.uk>
          University of Liverpool & STFC Rutherford Appleton Lab

          Jim Dobson <J.Dobson07 \at imperial.ac.uk>
          Imperial College London

\created  Aug 1, 2009

\cpright  Copyright (c) 2003-2018, The GENIE Collaboration
          For the full text of the license visit http://copyright.genie-mc.org
          or see $GENIE/LICENSE
*/
//____________________________________________________________________________

#ifndef _G_REWEIGHT_NON_RES_BKG_H_
#define _G_REWEIGHT_NON_RES_BKG_H_

#include <map>

#include "Tools/ReWeight/GReWeightModel.h"

namespace genie {
namespace rew   {

 class GReWeightNonResonanceBkg : public GReWeightModel 
 {
 public:
   GReWeightNonResonanceBkg();
  ~GReWeightNonResonanceBkg();

   // implement the GReWeightI interface
   bool   AppliesTo      (ScatteringType_t type, bool is_cc) const;
   bool   IsHandled      (GSyst_t syst) const;
   void   SetSystematic  (GSyst_t syst, double val);
   void   Reset          (void);
   void   Reconfigure    (void);
   double CalcWeight     (const EventRecord & event);

   // various config options
   void SetWminCut (double W ) { fWmin = W; }

 private:

   void Init (void);

   double fWmin;   ///< W_{min} cut. Reweight only events with W < W_{min}

   std::map<GSyst_t, double> fRTwkDial;
   std::map<GSyst_t, double> fRDef;
   std::map<GSyst_t, double> fRCurr;
 };

} // rew   namespace
} // genie namespace

#endif

