//____________________________________________________________________________
/*
 Copyright (c) 2003-2006, GENIE Neutrino MC Generator Collaboration
 All rights reserved.
 For the licensing terms see $GENIE/USER_LICENSE.

 Author: Costas Andreopoulos <C.V.Andreopoulos@rl.ac.uk>
         CCLRC, Rutherford Appleton Laboratory - November 17, 2004

 For the class documentation see the corresponding header file.

 Important revisions after version 2.0.0 :

*/
//____________________________________________________________________________

#include <algorithm>

#include <TParticlePDG.h>
#include <TMCParticle6.h>

#include "BaryonResonance/BaryonResUtils.h"
#include "Conventions/Constants.h"
#include "Decay/DecayModelI.h"
#include "EVGModules/UnstableParticleDecayer.h"
#include "GHEP/GHepStatus.h"
#include "GHEP/GHepParticle.h"
#include "GHEP/GHepRecord.h"
#include "Messenger/Messenger.h"
#include "PDG/PDGLibrary.h"
#include "PDG/PDGCodes.h"
#include "PDG/PDGUtils.h"

using std::count;

using namespace genie;
using namespace genie::constants;

//___________________________________________________________________________
UnstableParticleDecayer::UnstableParticleDecayer() :
EventRecordVisitorI("genie::UnstableParticleDecayer")
{

}
//___________________________________________________________________________
UnstableParticleDecayer::UnstableParticleDecayer(string config) :
EventRecordVisitorI("genie::UnstableParticleDecayer", config)
{

}
//___________________________________________________________________________
UnstableParticleDecayer::~UnstableParticleDecayer()
{

}
//___________________________________________________________________________
void UnstableParticleDecayer::ProcessEventRecord(GHepRecord * evrec) const
{
  //-- Loop over particles, find unstable ones and decay them

  TObjArrayIter piter(evrec);
  GHepParticle * p = 0;
  unsigned int ipos = 0;

  while( (p = (GHepParticle *) piter.Next()) ) {

     if( this->ToBeDecayed(p) ) {
        LOG("ParticleDecayer", pNOTICE)
              << "Decaying unstable particle: " << p->Name();

        //-- Get the parent particle 4-momentum

        TLorentzVector p4(p->Px(), p->Py(), p->Pz(), p->E());

        //-- Decay it & retrieve the decay products
        //   The decayer might not be able to handle it - in which case it
        //   should return a NULL decay products container

        DecayerInputs_t dinp;

        dinp.PdgCode = p->Pdg();
        dinp.P4      = &p4;

        TClonesArray * decay_products = fDecayer->Decay(dinp);

        //-- Check whether the particle was decayed
        if(decay_products) {
           LOG("ParticleDecayer", pINFO) << "The particle was decayed";

           //-- Mark it as a 'decayed state' & add its daughter links
           p->SetStatus(kIStDecayedState);

           //-- Loop over the daughter and add them to the event record
           this->CopyToEventRecord(decay_products, evrec, ipos);

           decay_products->Delete();
           delete decay_products;
        }// !=0
     }// to be decayed?
     ipos++;

  } // loop over particles
}
//___________________________________________________________________________
bool UnstableParticleDecayer::ToBeDecayed(GHepParticle * particle) const
{
   if( particle->Pdg() != 0 &&
                particle->Status() == kIStStableFinalState)
                                           return this->IsUnstable(particle);
   return false;
}
//___________________________________________________________________________
bool UnstableParticleDecayer::IsUnstable(GHepParticle * particle) const
{
  int pdg_code = particle->Pdg();

  TParticlePDG * ppdg = PDGLibrary::Instance()->Find(pdg_code);

  if( ppdg->Lifetime() < fMaxLifetime ) { /*return true*/ };

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - temporary/
  // ROOT's TParticlepdg::Lifetime() does not work properly
  // do something else instead (temporarily)

  int particles_to_decay[] = {
          kPdgPi0, 
          kPdgDP, kPdgDM, kPdgD0, kPdgAntiD0, kPdgDPs, kPdgDMs,
          kPdgLambdaPc, kPdgSigmaPc, kPdgSigmaPPc };

  const int N = sizeof(particles_to_decay) / sizeof(int);

  int matches = count(particles_to_decay, particles_to_decay+N, pdg_code);

  if(matches > 0 || utils::res::IsBaryonResonance(pdg_code)) return true;
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - /temporary

  return false;
}
//___________________________________________________________________________
void UnstableParticleDecayer::CopyToEventRecord(TClonesArray *
                decay_products, GHepRecord * evrec, int mother_pos) const
{
  TMCParticle * dpmc = 0;

  TObjArrayIter decay_iter(decay_products);

  while( (dpmc = (TMCParticle *) decay_iter.Next()) ) {

     TLorentzVector vdummy(0,0,0,0); // position 4-vector

     double px = dpmc->GetPx();
     double py = dpmc->GetPy();
     double pz = dpmc->GetPz();
     double E  = dpmc->GetEnergy();

     TLorentzVector p4(px, py, pz, E); // momentum 4-vector

     int          pdg    = dpmc->GetKF();
     GHepStatus_t status = GHepStatus_t (dpmc->GetKS());

     //-- Only add the decay products - the mother particle already exists
     //   (mother's daughter list will be automatically updated with every
     //    AddParticle() call)
     if(status == kIStStableFinalState) {
         evrec->AddParticle(pdg, status, mother_pos,-1,-1,-1, p4, vdummy);
     }
  }
}
//___________________________________________________________________________
void UnstableParticleDecayer::Configure(const Registry & config)
{
  Algorithm::Configure(config);
  this->LoadConfig();
}
//___________________________________________________________________________
void UnstableParticleDecayer::Configure(string config)
{
  Algorithm::Configure(config);
  this->LoadConfig();
}
//___________________________________________________________________________
void UnstableParticleDecayer::LoadConfig(void)
{
  //-- Get the specified maximum lifetime tmax (decay with lifetime < tmax)
  fMaxLifetime = fConfig->GetDoubleDef("max-lifetime-for-unstables", 1e-10);

  //-- Get the specified decay model
  fDecayer = dynamic_cast<const DecayModelI *>
                      (this->SubAlg("decayer-alg-name","decayer-param-set"));

  assert(fDecayer);
}
//___________________________________________________________________________
