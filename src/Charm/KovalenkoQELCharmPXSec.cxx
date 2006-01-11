//____________________________________________________________________________
/*!

\class    genie::KovalenkoQELCharmPXSec

\brief    Computes the QEL Charm Production Differential Cross Section
          using \b Kovalenko's duality model approach.

          The computed differential cross section is the Dxsec = dxsec/dQ^2
          where \n
            \li \c Q2 is the momentum transfer.

          It models the differential cross sections for: \n
             \li v + n \rightarrow mu- + Lambda_{c}^{+} (2285)
             \li v + n \rightarrow mu- + Sigma_{c}^{+}  (2455)
             \li v + p \rightarrow mu- + Sigma_{c}^{++} (2455)

          Is a concrete implementation of the XSecAlgorithmI interface.

\ref      S.G.Kovalenko, Sov.J.Nucl.Phys.52:934 (1990)

\author   Costas Andreopoulos <C.V.Andreopoulos@rl.ac.uk>
          CCLRC, Rutherford Appleton Laboratory

\created  June 10, 2004

*/
//____________________________________________________________________________

#include <iostream>

#include <TMath.h>

#include "Algorithm/AlgFactory.h"
#include "Charm/KovalenkoQELCharmPXSec.h"
#include "Conventions/Constants.h"
#include "Conventions/RefFrame.h"
#include "Conventions/Units.h"
#include "Messenger/Messenger.h"
#include "Numerical/UnifGrid.h"
#include "Numerical/FunctionMap.h"
#include "Numerical/IntegratorI.h"
#include "PDF/PDF.h"
#include "PDF/PDFModelI.h"
#include "PDG/PDGCodes.h"
#include "PDG/PDGUtils.h"
#include "PDG/PDGLibrary.h"

using namespace genie;
using namespace genie::constants;

//____________________________________________________________________________
KovalenkoQELCharmPXSec::KovalenkoQELCharmPXSec() :
XSecAlgorithmI("genie::KovalenkoQELCharmPXSec")
{

}
//____________________________________________________________________________
KovalenkoQELCharmPXSec::KovalenkoQELCharmPXSec(string config) :
XSecAlgorithmI("genie::KovalenkoQELCharmPXSec", config)
{

}
//____________________________________________________________________________
KovalenkoQELCharmPXSec::~KovalenkoQELCharmPXSec()
{

}
//____________________________________________________________________________
double KovalenkoQELCharmPXSec::XSec(const Interaction * interaction) const
{
  LOG("CharmXSec", pDEBUG) << *fConfig;

  if(! this -> ValidProcess    (interaction) ) return 0.;
  if(! this -> ValidKinematics (interaction) ) return 0.;

  //----- get kinematics & init state - compute auxiliary vars
  const Kinematics &   kinematics  = interaction->GetKinematics();
  const InitialState & init_state  = interaction->GetInitialState();

  //neutrino energy & momentum transfer
  double E   = init_state.GetProbeE(kRfStruckNucAtRest);
  double E2  = E * E;
  double Q2  = kinematics.Q2();

  //resonance mass & nucleon mass
  double MR  = this->MRes  (interaction);
  double MR2 = TMath::Power(MR,2);
  double Mnuc  = kNucleonMass;
  double Mnuc2 = Mnuc * Mnuc;

  //----- Calculate the differential cross section dxsec/dQ^2
  double Gf        = kGF_2 / (2*kPi);
  double vR        = (MR2 - Mnuc2 + Q2) / (2*Mnuc);
  double xiR       = this->xiBar(interaction, vR);
  double vR2       = vR*vR;
  double vR_E      = vR/E;
  double Q2_4E2    = Q2/(4*E2);
  double Q2_2MExiR = Q2/(2*Mnuc*E*xiR);
  double Z         = this->ZR(interaction);
  double D         = this->DR(interaction);

  LOG("CharmXSec", pDEBUG) << "Z = " << Z << ", D = " << D;

  double xsec = Gf*Z*D * (1 - vR_E + Q2_4E2 + Q2_2MExiR) *
                                     TMath::Sqrt(vR2 + Q2) / (vR*xiR);
  return xsec;
}
//____________________________________________________________________________
double KovalenkoQELCharmPXSec::ZR(const Interaction * interaction) const
{
  double Mo2   = fMo*fMo;
  double Mnuc2 = kNucleonMass_2;
  double MR    = this->MRes(interaction);
  double MR2   = TMath::Power(MR,2.);
  double D0    = this->DR(interaction, true); // D^R(Q^2=0)
  double sumF2 = this->SumF2(interaction);    // FA^2+F1^2

  double Z  = 2*Mo2*kSin8c_2 * sumF2 / (D0 * (MR2-Mnuc2));
  return Z;
}
//____________________________________________________________________________
double KovalenkoQELCharmPXSec::DR(
                             const Interaction * interaction, bool norm) const
{
  //----- compute PDFs
  PDF pdfs;
  pdfs.SetModel(fPDFModel);   // <-- attach algorithm

  //----- compute integration area = [xi_bar_plus, xi_bar_minus]
  const Kinematics & kinematics = interaction->GetKinematics();

  double Q2     = kinematics.Q2();
  double Mnuc   = kNucleonMass;
  double Mnuc2  = kNucleonMass_2;
  double MR     = this->MRes(interaction);
  double DeltaR = this->ResDM(interaction);

  double vR_minus  = ( TMath::Power(MR-DeltaR,2) - Mnuc2 + Q2 ) / (2*Mnuc);
  double vR_plus   = ( TMath::Power(MR+DeltaR,2) - Mnuc2 + Q2 ) / (2*Mnuc);

  LOG("CharmXSec", pDEBUG)
            << "vR = [plus: " << vR_plus << ", minus: " << vR_minus << "]";

  double xi_bar_minus = this->xiBar(interaction, vR_minus);
  double xi_bar_plus  = this->xiBar(interaction, vR_plus);

  LOG("CharmXSec", pDEBUG) << "Integration limits = ["
                             << xi_bar_plus << ", " << xi_bar_minus << "]";

  //----- define the integration grid & instantiate a FunctionMap
  UnifGrid grid;
  grid.AddDimension(fNBins, xi_bar_plus, xi_bar_minus);

  FunctionMap fmap(grid);

  //----- auxiliary variables
  const InitialState & init_state = interaction -> GetInitialState();

  double delta_xi_bar = (xi_bar_plus - xi_bar_minus) / (fNBins - 1);

  bool isP = pdg::IsProton ( init_state.GetTarget().StruckNucleonPDGCode() );

  //----- loop over x range (at fixed Q^2) & compute the function map
  for(int i = 0; i < fNBins; i++) {

     double t = xi_bar_plus + i * delta_xi_bar;

     if( t<0 || t>1) fmap.AddPoint( 0., i );
     else {
       if(norm) pdfs.Calculate(t, 0.);
       else     pdfs.Calculate(t, Q2);

       double f = (isP) ? ( pdfs.DownValence() + pdfs.DownSea() ):
                          ( pdfs.UpValence()   + pdfs.UpSea()   );
       fmap.AddPoint( f, i );

       LOG("CharmXSec", pDEBUG)
          << "point...." << i+1 << "/" << fNBins << " : "
          << "x*pdf(Q^2 = " << Q2 << ", x = " << t << ") = " << f;
    }
  }

  //----- Numerical integration
  double D = fIntegrator->Integrate(fmap);
  return D;
}
//____________________________________________________________________________
double KovalenkoQELCharmPXSec::xiBar(
                              const Interaction * interaction, double v) const
{
  const Kinematics & kinematics = interaction->GetKinematics();

  double Q2     = kinematics.Q2();
  double Mnuc   = kNucleonMass;
  double Mo2    = fMo*fMo;
  double v2     = v *v;

  LOG("CharmXSec", pDEBUG)
                     << "Q2 = " << Q2 << ", Mo = " << fMo << ", v = " << v;

  double xi  = (Q2/Mnuc) / (v + TMath::Sqrt(v2+Q2));
  double xib = xi * ( 1 + (1 + Mo2/(Q2+Mo2))*Mo2/Q2 );
  return xib;
}
//____________________________________________________________________________
double KovalenkoQELCharmPXSec::ResDM(const Interaction * interaction) const
{
// Resonance Delta M obeys the constraint DM(R+/-) <= |M(R+/-) - M(R)|
// where M(R-) <= M(R) <= M(R+) are the masses of the neighboring
// resonances R+, R-.
// Get the values from the algorithm conf. registry, and if they do not exist
// set them to default values (Eq.(20) in Sov.J.Nucl.Phys.52:934 (1990)

  const XclsTag & xcls = interaction->GetExclusiveTag();

  int pdgc = xcls.CharmHadronPDGCode();

  bool isLambda = (pdgc == kPdgLambdacP);
  bool isSigma  = (pdgc == kPdgSigmacP || pdgc == kPdgSigmacPP);

  if      ( isLambda ) return fResDMLambda;
  else if ( isSigma  ) return fResDMSigma;
  else                 abort();

  return 0;
}
//____________________________________________________________________________
double KovalenkoQELCharmPXSec::MRes(const Interaction * interaction) const
{
  const XclsTag & xcls = interaction->GetExclusiveTag();

  int pdgc  = xcls.CharmHadronPDGCode();
  double MR = PDGLibrary::Instance()->Find(pdgc)->Mass();
  return MR;
}
//____________________________________________________________________________
double KovalenkoQELCharmPXSec::vR_minus(const Interaction * interaction) const
{
  const Kinematics & kinematics = interaction->GetKinematics();

  double Q2  = kinematics.Q2();
  double dR  = this->ResDM(interaction);
  double MR  = MRes(interaction);
  double MN  = kNucleonMass;
  double MN2 = kNucleonMass_2;
  double vR  = (TMath::Power(MR-dR,2) - MN2 + Q2) / (2*MN);
  return vR;
}
//____________________________________________________________________________
double KovalenkoQELCharmPXSec::vR_plus(const Interaction * interaction) const
{
  const Kinematics & kinematics = interaction->GetKinematics();

  double Q2  = kinematics.Q2();
  double dR  = this->ResDM(interaction);
  double MR  = MRes(interaction);
  double MN  = kNucleonMass;
  double MN2 = kNucleonMass_2;
  double vR  = (TMath::Power(MR+dR,2) - MN2 + Q2) / (2*MN);
  return vR;
}
//____________________________________________________________________________
double KovalenkoQELCharmPXSec::SumF2(const Interaction * interaction) const
{
// Returns F1^2 (Q^2=0) + FA^2 (Q^2 = 0) for the normalization factor.
// Get the values from the algorithm conf. registry, and if they do not exist
// set them to default values I computed using Sov.J.Nucl.Phys.52:934 (1990)

  const XclsTag &      xcls       = interaction->GetExclusiveTag();
  const InitialState & init_state = interaction->GetInitialState();

  int pdgc = xcls.CharmHadronPDGCode();
  bool isP = pdg::IsProton ( init_state.GetTarget().StruckNucleonPDGCode() );
  bool isN = pdg::IsNeutron( init_state.GetTarget().StruckNucleonPDGCode() );

  if      ( pdgc == kPdgLambdacP && isN ) return fF2LambdaP;
  else if ( pdgc == kPdgSigmacP  && isN ) return fF2SigmaP;
  else if ( pdgc == kPdgSigmacPP && isP ) return fF2SigmaPP;
  else                                    abort();

  return 0;
}
//____________________________________________________________________________
bool KovalenkoQELCharmPXSec::ValidProcess(
                                        const Interaction * interaction) const
{
  //----- make sure we are dealing with one of the following channels:
  //
  //   v + n --> mu- + Lambda_{c}^{+} (2285)
  //   v + n --> mu- + Sigma_{c}^{+} (2455)
  //   v + p --> mu- + Sigma_{c}^{++} (2455)

  if(interaction->TestBit(kISkipProcessChk)) return true;

  const XclsTag &      xcls       = interaction->GetExclusiveTag();
  const InitialState & init_state = interaction->GetInitialState();
  const ProcessInfo &  proc_info  = interaction->GetProcessInfo();

  bool is_exclusive_charm = (xcls.IsCharmEvent() && !xcls.IsInclusiveCharm());
  if(!is_exclusive_charm) return false;

  if(!proc_info.IsQuasiElastic()) return false;
  if(!proc_info.IsWeak())          return false;

  bool isP = pdg::IsProton ( init_state.GetTarget().StruckNucleonPDGCode() );
  bool isN = pdg::IsNeutron( init_state.GetTarget().StruckNucleonPDGCode() );

  int pdgc = xcls.CharmHadronPDGCode();

  bool can_handle = (
         (pdgc == kPdgLambdacP && isN) || /* v + n -> l + #Lambda_{c}^{+} */
         (pdgc == kPdgSigmacP  && isN) || /* v + n -> l + #Sigma_{c}^{+}  */
         (pdgc == kPdgSigmacPP && isP)    /* v + p -> l + #Sigma_{c}^{++} */
  );
  return can_handle;
}
//____________________________________________________________________________
bool KovalenkoQELCharmPXSec::ValidKinematics(
                                        const Interaction * interaction) const
{
  if(interaction->TestBit(kISkipKinematicChk)) return true;

  //----- get kinematics & init state - compute auxiliary vars
  const Kinematics &   kinematics  = interaction->GetKinematics();
  const InitialState & init_state  = interaction->GetInitialState();

  //neutrino energy & momentum transfer
  double E   = init_state.GetProbeE(kRfStruckNucAtRest);
  double Q2  = kinematics.Q2();

  //resonance, final state primary lepton & nucleon mass
  double MR  = this -> MRes  (interaction);
  double ml    = interaction->GetFSPrimaryLepton()->Mass();
  double Mnuc  = kNucleonMass;
  double Mnuc2 = Mnuc * Mnuc;

  //resonance threshold
  double ER = ( TMath::Power(MR+ml,2) - Mnuc2 ) / (2*Mnuc);

  if(Q2 >= fQ2max || Q2 <= fQ2min) return false;
  if(E <= ER)                      return false;

  return true;
}
//____________________________________________________________________________
void KovalenkoQELCharmPXSec::Configure(const Registry & config)
{
  Algorithm::Configure(config);
  this->LoadConfigData();
  this->LoadSubAlg();
}
//____________________________________________________________________________
void KovalenkoQELCharmPXSec::Configure(string param_set)
{
  Algorithm::Configure(param_set);
  this->LoadConfigData();
  this->LoadSubAlg();
}
//____________________________________________________________________________
void KovalenkoQELCharmPXSec::LoadConfigData(void)
{
  // Get config values or set defaults
  fF2LambdaP   = fConfig->GetDoubleDef("F1^2+FA^2-LambdaP", 2.07);
  fF2SigmaP    = fConfig->GetDoubleDef("F1^2+FA^2-SigmaP",  0.71);
  fF2SigmaPP   = fConfig->GetDoubleDef("F1^2+FA^2-SigmaPP", 1.42);
  fResDMLambda = fConfig->GetDoubleDef("Res-DeltaM-Lambda", 0.56); /*GeV*/
  fResDMSigma  = fConfig->GetDoubleDef("Res-DeltaM-Sigma",  0.20); /*GeV*/

  // 'proper scale of internal nucleon dynamics'.
  // In the original paper Mo = 0.08 +/- 0.02 GeV.
  fMo = fConfig->GetDoubleDef("Mo", 0.1);

  // read kinematic cuts from config
  fQ2min = fConfig->GetDoubleDef("Q2min", -999999);
  fQ2min = fConfig->GetDoubleDef("Q2max",  999999);

  assert(fQ2min < fQ2max);

  fNBins = fConfig->GetIntDef("nbins", 201);
  assert(fNBins>1);
}
//____________________________________________________________________________
void KovalenkoQELCharmPXSec::LoadSubAlg(void)
{
  fPDFModel   = 0;
  fIntegrator = 0;

  fPDFModel = dynamic_cast<const PDFModelI *> (
                              this->SubAlg("pdf-alg-name", "pdf-param-set"));
  assert(fPDFModel);

  string integrator = fConfig->GetStringDef("integrator","genie::Simpson1D");
  AlgFactory * algf = AlgFactory::Instance();
  fIntegrator = dynamic_cast<const IntegratorI *> (
                                             algf->GetAlgorithm(integrator));
  assert(fIntegrator);
}
//____________________________________________________________________________

