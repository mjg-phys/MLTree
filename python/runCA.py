#You can run this code from the command line (after Athena setup) with a command:
#python runCA.py --filesInput=<inputFileName> MLTree.NtupleName=<outputNtupleFileName>"

if __name__=="__main__":

    from AthenaConfiguration.AllConfigFlags import initConfigFlags
    from MLTree.MainCfg import __MLTree
    cfgFlags = initConfigFlags()
    cfgFlags.addFlagsCategory("MLTree",__MLTree)

    cfgFlags.Exec.MaxEvents=-1
    cfgFlags.Input.isMC=True
    #cfgFlags.Input.Files=["/home/markhodgkinson.linux/ESD.28115683._000440.pool.root.1"]    
    cfgFlags.Input.Files= ["/remote/nas00-0/shared/atlas/combined-performance/jet-met/PFlow/dataFiles/MLTreeNtuples/raw/mc16_13TeV/ESD.32878415._005770.pool.root.1"]
    cfgFlags.Concurrency.NumThreads=1
    # cfgFlags.fillFromArgs()
    cfgFlags.Calo.TopoCluster.doCalibHitMoments= True
    cfgFlags.lock()


    from MLTree.MainCfg import MainCfg
    cfg = MainCfg(cfgFlags)

    from MLTree.MLTreeMakerCfg import MLTreeMakerCfg
    cfg.merge(MLTreeMakerCfg(cfgFlags,
                          TrackContainer = "InDetTrackParticles",
                           CaloClusterContainer = "CaloCalTopoClusters",
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
                           RootStreamName = "OutputStream"))                         

    cfg.run()
