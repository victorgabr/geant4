//
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// *                                                                  *
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications,  and indicate your *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************
//
// neutron_hp -- source file
// J.P. Wellisch, Nov-1996
// A prototype of the low energy neutron transport model.
//
// 12-Apr-06 fix in delayed neutron and photon emission without FS data by T. Koi
// 07-Sep-11 M. Kelsey -- Follow change to G4HadFinalState interface

#include "G4NeutronHPFissionFS.hh"
#include "G4PhysicalConstants.hh"
#include "G4Nucleus.hh"
#include "G4DynamicParticleVector.hh"
#include "G4NeutronHPFissionERelease.hh"
#include "G4ParticleTable.hh"

 void G4NeutronHPFissionFS::Init (G4double A, G4double Z, G4int M, G4String & dirName, G4String & aFSType)
 {
    //G4cout << "G4NeutronHPFissionFS::Init " << A << " " << Z << " " << M << G4endl;
    theFS.Init(A, Z, M, dirName, aFSType);
    theFC.Init(A, Z, M, dirName, aFSType);
    theSC.Init(A, Z, M, dirName, aFSType);
    theTC.Init(A, Z, M, dirName, aFSType);
    theLC.Init(A, Z, M, dirName, aFSType);

    theFF.Init(A, Z, M, dirName, aFSType);
    if ( getenv("G4NEUTRONHP_PRODUCE_FISSION_FRAGMENTS") && theFF.HasFSData() ) 
    {
       G4cout << "Activate Fission Fragments Prodcution for the target isotope of " 
       << "Z = " << (G4int)Z
       << ", A = " << (G4int)A
       //<< "M = " << M
       << G4endl;
       G4cout << "As the result, delayed neutrons are omitted and they should be taken care by RadioaActiveDecay." 
       << G4endl;
       produceFissionFragments = true; 
    }
 }
 G4HadFinalState * G4NeutronHPFissionFS::ApplyYourself(const G4HadProjectile & theTrack)
 {  
 //G4cout << "G4NeutronHPFissionFS::ApplyYourself " << G4endl;
// prepare neutron
   theResult.Clear();
   G4double eKinetic = theTrack.GetKineticEnergy();
   const G4HadProjectile *incidentParticle = &theTrack;
   G4ReactionProduct theNeutron( const_cast<G4ParticleDefinition *>(incidentParticle->GetDefinition()) );
   theNeutron.SetMomentum( incidentParticle->Get4Momentum().vect() );
   theNeutron.SetKineticEnergy( eKinetic );

// prepare target
   G4Nucleus aNucleus;
   G4ReactionProduct theTarget; 
   G4double targetMass = theFS.GetMass();
   G4ThreeVector neuVelo = (1./incidentParticle->GetDefinition()->GetPDGMass())*theNeutron.GetMomentum();
   theTarget = aNucleus.GetBiasedThermalNucleus( targetMass, neuVelo, theTrack.GetMaterial()->GetTemperature());

// set neutron and target in the FS classes 
  theFS.SetNeutron(theNeutron);
  theFS.SetTarget(theTarget);
  theFC.SetNeutron(theNeutron);
  theFC.SetTarget(theTarget);
  theSC.SetNeutron(theNeutron);
  theSC.SetTarget(theTarget);
  theTC.SetNeutron(theNeutron);
  theTC.SetTarget(theTarget);
  theLC.SetNeutron(theNeutron);
  theLC.SetTarget(theTarget);


  theFF.SetNeutron(theNeutron);
  theFF.SetTarget(theTarget);

//TKWORK 120531
//G4cout << theTarget.GetDefinition() << G4endl; this should be NULL
//G4cout << "Z = " << theBaseZ << ", A = " << theBaseA << ", M = " << theBaseM << G4endl;
// theNDLDataZ,A,M should be filled in each FS (theFS, theFC, theSC, theTC, theLC and theFF)
////G4cout << "Z = " << theNDLDataZ << ", A = " << theNDLDataA << ", M = " << theNDLDataM << G4endl;

// boost to target rest system and decide on channel.
   theNeutron.Lorentz(theNeutron, -1*theTarget);

// dice the photons

   G4DynamicParticleVector * thePhotons;    
   thePhotons = theFS.GetPhotons();

// select the FS in charge

   eKinetic = theNeutron.GetKineticEnergy();    
   G4double xSec[4];
   xSec[0] = theFC.GetXsec(eKinetic);
   xSec[1] = xSec[0]+theSC.GetXsec(eKinetic);
   xSec[2] = xSec[1]+theTC.GetXsec(eKinetic);
   xSec[3] = xSec[2]+theLC.GetXsec(eKinetic);
   G4int it;
   unsigned int i=0;
   G4double random = G4UniformRand();
   if(xSec[3]==0) 
   {
     it=-1;
   }
   else
   {
     for(i=0; i<4; i++)
     {
       it =i;
       if(random<xSec[i]/xSec[3]) break;
     }
   }

// dice neutron multiplicities, energies and momenta in Lab. @@
// no energy conservation on an event-to-event basis. we rely on the data to be ok. @@
// also for mean, we rely on the consistancy of the data. @@

   G4int Prompt=0, delayed=0, all=0;
   G4DynamicParticleVector * theNeutrons = 0;
   switch(it) // check logic, and ask, if partials can be assumed to correspond to individual particles @@@
   {
     case 0:
       theFS.SampleNeutronMult(all, Prompt, delayed, eKinetic, 0);
       if(Prompt==0&&delayed==0) Prompt=all;
       theNeutrons = theFC.ApplyYourself(Prompt); // delayed always in FS 
       // take 'U' into account explicitely (see 5.4) in the sampling of energy @@@@
       break;
     case 1:
       theFS.SampleNeutronMult(all, Prompt, delayed, eKinetic, 1);
       if(Prompt==0&&delayed==0) Prompt=all;
       theNeutrons = theSC.ApplyYourself(Prompt); // delayed always in FS, off done in FSFissionFS
       break;
     case 2:
       theFS.SampleNeutronMult(all, Prompt, delayed, eKinetic, 2);
       if(Prompt==0&&delayed==0) Prompt=all;
       theNeutrons = theTC.ApplyYourself(Prompt); // delayed always in FS
       break;
     case 3:
       theFS.SampleNeutronMult(all, Prompt, delayed, eKinetic, 3);
       if(Prompt==0&&delayed==0) Prompt=all;
       theNeutrons = theLC.ApplyYourself(Prompt); // delayed always in FS
       break;
     default:
       break;
   }

// dice delayed neutrons and photons, and fallback 
// for Prompt in case channel had no FS data; add all paricles to FS.

   //TKWORK120531
   if ( produceFissionFragments ) delayed=0;

   G4double * theDecayConstants;

   if( theNeutrons != 0)
   {
     theDecayConstants = new G4double[delayed];
     //
     //110527TKDB  Unused codes, Detected by gcc4.6 compiler 
     //G4int nPhotons = 0;
     //if(thePhotons!=0) nPhotons = thePhotons->size();
     for(i=0; i<theNeutrons->size(); i++)
     {
       theResult.AddSecondary(theNeutrons->operator[](i));
     }
     delete theNeutrons;  

     G4DynamicParticleVector * theDelayed = 0;
//   G4cout << "delayed" << G4endl;
     theDelayed = theFS.ApplyYourself(0, delayed, theDecayConstants);
     for(i=0; i<theDelayed->size(); i++)
     {
       G4double time = -std::log(G4UniformRand())/theDecayConstants[i];
       time += theTrack.GetGlobalTime();
       theResult.AddSecondary(theDelayed->operator[](i));
       theResult.GetSecondary(theResult.GetNumberOfSecondaries()-1)->SetTime(time);
     }
     delete theDelayed;                  
   }
   else
   {
//    cout << " all = "<<all<<G4endl;
     theFS.SampleNeutronMult(all, Prompt, delayed, eKinetic, 0);
     theDecayConstants = new G4double[delayed];
     if(Prompt==0&&delayed==0) Prompt=all;
     theNeutrons = theFS.ApplyYourself(Prompt, delayed, theDecayConstants);
     //110527TKDB  Unused codes, Detected by gcc4.6 compiler 
     //G4int nPhotons = 0;
     //if(thePhotons!=0) nPhotons = thePhotons->size();
     G4int i0;
     for(i0=0; i0<Prompt; i0++)
     {
       theResult.AddSecondary(theNeutrons->operator[](i0));
     }

//G4cout << "delayed" << G4endl;
     for(i0=Prompt; i0<Prompt+delayed; i0++)
     {
       G4double time = -std::log(G4UniformRand())/theDecayConstants[i0-Prompt];
       time += theTrack.GetGlobalTime();        
       theResult.AddSecondary(theNeutrons->operator[](i0));
       theResult.GetSecondary(theResult.GetNumberOfSecondaries()-1)->SetTime(time);
     }
     delete theNeutrons;   
   }
   delete [] theDecayConstants;
//    cout << "all delayed "<<delayed<<G4endl; 
   unsigned int nPhotons = 0;
   if(thePhotons!=0)
   {
     nPhotons = thePhotons->size();
     for(i=0; i<nPhotons; i++)
     {
       theResult.AddSecondary(thePhotons->operator[](i));
     }
     delete thePhotons; 
   }

// finally deal with local energy depositions.
//    G4cout <<"Number of secondaries = "<<theResult.GetNumberOfSecondaries()<< G4endl;
//    G4cout <<"Number of photons = "<<nPhotons<<G4endl;
//    G4cout <<"Number of Prompt = "<<Prompt<<G4endl;
//    G4cout <<"Number of delayed = "<<delayed<<G4endl;

   G4NeutronHPFissionERelease * theERelease = theFS.GetEnergyRelease();
   G4double eDepByFragments = theERelease->GetFragmentKinetic();
   //theResult.SetLocalEnergyDeposit(eDepByFragments);
   if ( !produceFissionFragments ) theResult.SetLocalEnergyDeposit(eDepByFragments);
//    cout << "local energy deposit" << eDepByFragments<<G4endl;
// clean up the primary neutron
   theResult.SetStatusChange(stopAndKill);
   //G4cout << "Prompt = " << Prompt << ", Delayed = " << delayed << ", All= " << all << G4endl;
   //G4cout << "local energy deposit " << eDepByFragments/MeV << "MeV " << G4endl;

   //TKWORK120531
   if ( produceFissionFragments )
   {
      G4int fragA_Z=0;
      G4int fragA_A=0;
      G4int fragA_M=0;
      // System is traget rest!
      theFF.GetAFissionFragment(eKinetic,fragA_Z,fragA_A,fragA_M);
      G4int fragB_Z=(G4int)theBaseZ-fragA_Z;
      G4int fragB_A=(G4int)theBaseA-fragA_A-Prompt;
      //fragA_M ignored 
      //G4int fragB_M=theBaseM-fragA_M; 
      //G4cout << fragA_Z << " " << fragA_A << " " << fragA_M << G4endl;
      //G4cout << fragB_Z << " " << fragB_A << G4endl;

      G4ParticleTable* pt = G4ParticleTable::GetParticleTable();
      //Excitation energy is not taken into account 
      G4ParticleDefinition* pdA = pt->GetIon( fragA_Z , fragA_A , 0.0 );
      G4ParticleDefinition* pdB = pt->GetIon( fragB_Z , fragB_A , 0.0 );

      //Isotropic Distribution 
      G4double phi = twopi*G4UniformRand();
      G4double theta = pi*G4UniformRand();
      G4double sinth = std::sin(theta);
      G4ThreeVector direction (sinth*std::cos(phi) , sinth*std::sin(phi), std::cos(theta) );

      // Just use ENDF value for this 
      G4double ER = eDepByFragments; 
      G4double ma = pdA->GetPDGMass();
      G4double mb = pdB->GetPDGMass();
      G4double EA = ER / ( 1 + ma/mb);
      G4double EB = ER - EA;
      G4DynamicParticle* dpA = new G4DynamicParticle( pdA , direction , EA);
      G4DynamicParticle* dpB = new G4DynamicParticle( pdB , -direction , EB);
      theResult.AddSecondary(dpA);
      theResult.AddSecondary(dpB);
   }
   //TKWORK 120531 END

   return &theResult;
 }