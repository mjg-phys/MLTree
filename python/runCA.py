from AthenaConfiguration.ComponentFactory import CompFactory

#You can run this code from the command line (after Athena setup) with a command:
#python runCA.py --filesInput=<inputFileName> MLTree.NtupleName=<outputNtupleFileName>"

def __MLTree():
  from MLTree.MLTreeConfigFlags import createMLTreeConfigFlags
  return createMLTreeConfigFlags()

if __name__=="__main__":

    from AthenaConfiguration.AllConfigFlags import ConfigFlags as cfgFlags
    cfgFlags.addFlagsCategory("MLTree",__MLTree)
      

    cfgFlags.Exec.MaxEvents=-1
    cfgFlags.Input.isMC=True
    cfgFlags.Input.Files=["/home/markhodgkinson.linux/ESD.28115683._000440.pool.root.1"]
    #Do not set either of these - I do not understand why, but MLTreeMaker will not work with either adjusted from defaults.
    cfgFlags.Concurrency.NumThreads=1
    #cfgFlags.Concurrency.NumProcs=1    
    cfgFlags.fillFromArgs()
    cfgFlags.lock()

    from AthenaConfiguration.MainServicesConfig import MainServicesCfg
    cfg = MainServicesCfg(cfgFlags)

    StoreGateSvc=CompFactory.StoreGateSvc
    cfg.addService(StoreGateSvc("DetectorStore"))

    histSvc = CompFactory.THistSvc(Output = ["OutputStream DATAFILE='"+ cfgFlags.MLTree.NtupleName+"', OPT='RECREATE'"])
    cfg.addService(histSvc)

    from AthenaPoolCnvSvc.PoolReadConfig import PoolReadCfg
    cfg.merge(PoolReadCfg(cfgFlags))

    #Configure topocluster algorithmsm, and associated conditions
    from CaloRec.CaloTopoClusterConfig import CaloTopoClusterCfg
    cfg.merge(CaloTopoClusterCfg(cfgFlags))

    #Given we rebuild topoclusters above, we must also rerun pflow
    #because when the topoclusters update then the links from FlowElement
    #to topocluster can become invalid. Rerunning pflow using
    #the rebuilt topoclusters solves this.
    #Note that the below config does not rebuild all Global PFlow links
    #which are not used in MLTreeMaker, so are not relevant in this context.
    from eflowRec.PFRun3Config import PFFullCfg
    cfg.merge(PFFullCfg(cfgFlags))
     
    from eflowRec.PFRun3Remaps import ListRemaps
 
    list_remaps=ListRemaps()
    for mapping in list_remaps:
      cfg.merge(mapping)    
         
    #decorate the topoclusters with calib hit calculations
    from CaloCalibHitRec.CaloCalibHitDecoratorCfg import CaloCalibHitDecoratorCfg 
    cfg.merge(CaloCalibHitDecoratorCfg(cfgFlags))
    cfg.getEventAlgo("CaloCalibClusterDecoratorAlgorithm").CaloClusterWriteDecorHandleKey_NLeadingTruthParticles = "CaloTopoClusters."+cfgFlags.Calo.TopoCluster.CalibrationHitDecorationName

    from TrackToCalo.TrackToCaloConfig import ParticleCaloExtensionToolCfg
    pcExtensionTool = cfg.popToolsAndMerge(ParticleCaloExtensionToolCfg(cfgFlags))
    
    #This provides the same selection cuts as used in MLTreeMaker, so we can use the pflow selector tool
    from InDetConfig.InDetTrackSelectionToolConfig import PFTrackSelectionToolCfg
    from AthenaCommon.Constants import INFO, DEBUG
    MLTreeMaker = CompFactory.MLTreeMaker(TrackContainer = "InDetTrackParticles",
                           CaloClusterContainer = "CaloCalTopoClusters",
                           Prefix = "CALO",
                           ClusterEmin = 0.0,
                           ClusterEmax = 2000.0,
                           ClusterEtaAbsmax = 3.0,
                           EventCleaning = False,
                           Tracking = True,
                           Pileup = False,
                           Clusters = True,
                           ClusterCells = True,
                           ClusterCalibHits = True,
                           ClusterCalibHitsPerCell = False,
                           ClusterMoments = True,
                           UncalibratedClusters = True,
                           TruthParticles = True,
                           EventTruth = False,
                           OnlyStableTruthParticles = False,
                           G4TruthParticles = False,
                           Jets = False,
                           JetContainers = ["AntiKt4EMTopoJets","AntiKt4LCTopoJets","AntiKt4TruthJets"],
                           OutputLevel = INFO,
                           TheTrackExtrapolatorTool=pcExtensionTool,
                           TrackSelectionTool=cfg.popToolsAndMerge(PFTrackSelectionToolCfg(cfgFlags)))

    #Has to be added in this convoluted way so that we can read EventInfo from the input file
    from AthenaConfiguration.ComponentAccumulator import ComponentAccumulator
    MLTreeAC = ComponentAccumulator()
    MLTreeAC.addEventAlgo(MLTreeMaker)
    cfg.merge(MLTreeAC)
    cfg.getEventAlgo("MLTreeMaker").RootStreamName = "OutputStream"
    cfg.getEventAlgo("MLTreeMaker").TrackSelectionTool.CutLevel = "TightPrimary"    


    cfg.run()
