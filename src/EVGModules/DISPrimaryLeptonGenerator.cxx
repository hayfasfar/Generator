//____________________________________________________________________________
/*
 Copyright (c) 2003-2006, GENIE Neutrino MC Generator Collaboration
 All rights reserved.
 For the licensing terms see $GENIE/USER_LICENSE.

 Author: Costas Andreopoulos <C.V.Andreopoulos@rl.ac.uk>
         CCLRC, Rutherford Appleton Laboratory - October 03, 2004

 For the class documentation see the corresponding header file.

 Important revisions after version 2.0.0 :

*/
//____________________________________________________________________________

#include <TMath.h>

#include "EVGModules/DISPrimaryLeptonGenerator.h"
#include "GHEP/GHepRecord.h"
#include "Interaction/Interaction.h"

using namespace genie;

//___________________________________________________________________________
DISPrimaryLeptonGenerator::DISPrimaryLeptonGenerator() :
PrimaryLeptonGenerator("genie::DISPrimaryLeptonGenerator")
{

}
//___________________________________________________________________________
DISPrimaryLeptonGenerator::DISPrimaryLeptonGenerator(string config) :
PrimaryLeptonGenerator("genie::DISPrimaryLeptonGenerator", config)
{

}
//___________________________________________________________________________
DISPrimaryLeptonGenerator::~DISPrimaryLeptonGenerator()
{

}
//___________________________________________________________________________
void DISPrimaryLeptonGenerator::ProcessEventRecord(
                                              GHepRecord * event_rec) const
{
// This method generates the final state primary lepton

  //-- Get the interaction & initial state objects
  Interaction * interaction = event_rec->GetInteraction();
  const InitialState & init_state = interaction->GetInitialState();

  //-- Figure out the Final State Lepton PDG Code
  int pdgc = interaction->GetFSPrimaryLepton()->PdgCode();

  //-- Use selected kinematics
  interaction->GetKinematicsPtr()->UseSelectedKinematics();

  //-- DIS Kinematics: Compute the lepton energy and the scattering
  //   angle with respect to the incoming neutrino

  //auxiliary params:
  double Ev   = init_state.GetProbeE(kRfStruckNucAtRest);
  double x    = interaction->GetKinematics().x();
  double y    = interaction->GetKinematics().y();
  double M    = init_state.GetTarget().StruckNucleonP4()->M();
  double ml   = interaction->GetFSPrimaryLepton()->Mass();
  double M2   = TMath::Power(M, 2);
  double ml2  = TMath::Power(ml,2);
  double Q2   = 2*x*y*M*Ev;
  double W2   = M2 + 2*M*Ev*y*(1-x);

  //Compute outgoing lepton energy
  double El  = Ev - 0.5 * (W2 - M2 + Q2) / M;

  //Compute outgoing lepton scat. angle with respect to the incoming v
  double pl  = TMath::Sqrt( TMath::Max(0., El*El-ml2) );
  assert(pl > 0);
  double cThSc = (El - 0.5*(Q2+ml2)/Ev) / pl; // cos(theta-scat) [-1,1]
  assert( TMath::Abs(cThSc) <= 1 );

  //-- Rotate its 4-momentum to the nucleon rest frame
  //   unit' = R(Theta0,Phi0) * R(ThetaSc,PhiSc) * R^-1(Theta0,Phi0) * unit
  TLorentzVector * pl4 = P4InNucRestFrame(event_rec, cThSc, El);

  //-- Boost it to the lab frame
  TVector3 * beta = NucRestFrame2Lab(event_rec);
  pl4->Boost(*beta); // active Lorentz transform
  delete beta;

  //-- Create a GHepParticle and add it to the event record
  //   (use the insertion method at the base PrimaryLeptonGenerator visitor)
  this->AddToEventRecord(event_rec, pdgc, pl4);

  delete pl4;

  //-- Set final state lepton polarization
  this->SetPolarization(event_rec);

  //-- Reset running kinematics
  interaction->GetKinematicsPtr()->ClearRunningValues();
}
//___________________________________________________________________________
