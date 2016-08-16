#!/usr/bin/env python

################################################################################
# A GENERAL EXPLANATION

"""
refinement.py

This script performs simple high-resolution (fullatom) refinement on an input
pose. The local conformation space is sampled using small backbone torsion angle
perturbations followed by backbone torsion angle minimization and sidechain
packing. The Rosetta standard score function evaluations are used to accept or
reject proposed structures. Some increases in score are accepted to escape
local minima.

Instructions:

1) ensure that your PDB file is in the current directory
2) run the script:
    from commandline                        >python D070_Refinement.py

    from within python/ipython              [1]: run D070_Refinement.py

Author: Evan H. Baugh
    revised and motivated by Robert Schleif

Updated by Boon Uranukul, 6/9/12
Simplified special constant seed initialization ~ Labonte

References:
    P. Bradley, K. Misura, and D. Baker, "Toward high-resolution de novo
        structure prediction for small proteins," Science 309 (5742)
        1868-1871 (2005).

"""

################################################################################
# THE BASIC PROTOCOL, sample_refinement

"""
This sample script is setup for usage with
    commandline arguments,
    default running within a python interpreter,
    or for import within a python interpreter,
        (exposing the sample_refinement method)

The method sample_refinement:
1.  creates a pose from the desired PDB file
2.  creates a (fullatom) reference copy of the pose
3.  creates a standard fullatom ScoreFunction
4.  creates a MoveMap with all backbone torsion angles free
5.  sets up a SmallMover for small backbone torsion angle perturbations
6.  sets up a ShearMover for small backbone torsion angle perturbations
7.  sets up a MinMover for backbone torsion minimization
8.  sets up a PackRotamersMover for sidechain packing
9.  create a PyMOL_Mover for viewing intermediate output
10. export the original structure, and scores, to PyMOL
11. sets up a RepeatMover on a TrialMover of a SequenceMover
        -setup the TrialMover
            a.  create a SequenceMover with the:
                    >SmallMover
                    >ShearMover
                    >MinMover
                    >PackRotamersMover
                    >PyMOL_Mover
            b.  create a MonteCarlo object for assessing moves
            c.  create the TrialMover (on the SequenceMover)
        -create the RepeatMover (on the TrialMover)
12.  creates a (Py)JobDistributor for managing multiple trajectories
13.  stores the original score evaluation
14.  performs the refinement protocol, for each trajectory:
         a. set necessary variables for the new trajectory
             -reload the starting pose
             -change the pose's PDBInfo.name, for the PyMOL_Mover
             -reset the MonteCarlo object
         b. perform the sampling and assessment using the RepeatMover
         c. output the (lowest scoring) decoy structure
             -output the decoy structure using the PyJobDistributor
             -export the decoy structure to PyMOL
             -store the decoy score
15.  outputs the score evaluations

"""

import optparse    # for sorting options

from rosetta import *
init(extra_options = "-constant_seed")
# normally, init() works fine
# for this sample script, we want to ease comparison by making sure all random
#    variables generated by Rosetta in this instance of PyRosetta start from a
#    constant seed
# here we provide the additional argument "-constant_seed" which sets all the
#    random variables generated by Rosetta from a constant seed (google random
#    seed for more information)
# some options can be set after initialization, please see PyRosetta.org FAQs
#    for more information
# WARNING: option '-constant_seed' is for testing only! MAKE SURE TO REMOVE IT IN PRODUCTION RUNS!!!!!

import os; os.chdir('.test.output')


#########
# Methods

def sample_refinement(pdb_filename,
        kT = 1.0, smallmoves = 3, shearmoves = 5,
        backbone_angle_max = 7, cycles = 9,
        jobs = 1, job_output = 'refine_output'):
    """
    Performs fullatom structural refinement on the input  <pdb_filename>  by
        perturbing backbone torsion angles with a maximum perturbation of
        <backbone_angle_max>  for  <cycles>  trials of
        <smallmoves>  perturbations of a random residue's phi or psi and
        <shearmoves>  perturbations of a random residue's phi and the preceding
        residue's psi followed by gradient based backbone torsion angle
        minimization and sidechain packing with an acceptance criteria scaled
        by  <kT>.  <jobs>  trajectories are performed, continually exporting
        structures to a PyMOL instance.
        Output structures are named  <job_output>_(job#).pdb.
    """
    # 1. create a pose from the desired PDB file
    pose = Pose()
    pose_from_file(pose, pdb_filename)

    # 2. create a reference copy of the pose in fullatom
    starting_pose = Pose()
    starting_pose.assign(pose)

    # 3. create a standard ScoreFunction
    #### implement the desired ScoreFunction here
    scorefxn = get_fa_scorefxn() #  create_score_function('standard')

    #### If you wish to use the ClassRelax protocol, uncomment the following
    ####    line and comment-out the protocol setup below
    #refinement = ClassicRelax( scorefxn )

    #### Setup custom high-resolution refinement protocol
    #### backbone refinement protocol

    # 4. create a MoveMap, all backbone torsions free
    movemap = MoveMap()
    movemap.set_bb(True)

    # 5. create a SmallMover
    # a SmallMover perturbs a random (free in the MoveMap) residue's phi or psi
    #    torsion angle for an input number of times and accepts of rejects this
    #    change based on the Metropolis Criteria using the "rama" ScoreType and
    #    the parameter kT
    # set the maximum angle to backbone_angle_max, apply it smallmoves times
    smallmover = SmallMover(movemap, kT, smallmoves)
    # angle_max is secondary structure dependent, however secondary structure
    #    has not been evaulated in this protocol, thus they are all set
    #    to the same value0
    smallmover.angle_max(backbone_angle_max)    # sets all at once
    #### use the overloaded version of the SmallMover.angle_max method if you
    ####    want to use secondary structure biased moves
    #smallmover.angle_max('H', backbone_angle_max)
    #smallmover.angle_max('E', backbone_angle_max)
    #smallmover.angle_max('L', backbone_angle_max)

    # 6. create a ShearMover
    # a ShearMover is identical to a SmallMover except that the angles perturbed
    #    are instead a random (free in the MoveMap) residue's phi and the
    #    preceding residue's psi, this reduces the downstream structural change
    # set the maximum angle to backbone_angle_max, apply it shearmoves times
    shearmover = ShearMover(movemap, kT, shearmoves)
    # same angle_max restictions as SmallMover
    shearmover.angle_max(backbone_angle_max)
    #### use the overloaded version of the SmallMover.angle_max method if you
    ####    want to use secondary structure biased moves
    #shearmover.angle_max('H', backbone_angle_max)
    #shearmover.angle_max('E', backbone_angle_max)
    #shearmover.angle_max('L', backbone_angle_max)

    # 7. create a MinMover, for backbone torsion minimization
    minmover = MinMover()
    minmover.movemap(movemap)
    minmover.score_function(scorefxn)

    #### sidechain refinement protocol, simple packing

    # 8. setup a PackRotamersMover
    to_pack = standard_packer_task(starting_pose)
    to_pack.restrict_to_repacking()    # prevents design, packing only
    to_pack.or_include_current(True)    # considers the original sidechains
    packmover = PackRotamersMover(scorefxn, to_pack)

    #### assess the new structure
    # 9. create a PyMOL_Mover
    pymover = PyMOL_Mover()
    # uncomment the line below to load structures into successive states
    #pymover.keep_history(True)
    #### the PyMOL_Mover slows down the protocol SIGNIFICANTLY but provides
    ####    very informative displays
    #### the keep_history flag (when True) tells the PyMOL_Mover to store new
    ####    structures into successive states, for a single trajectory, this
    ####    allows you to see intermediate changes (depending on where the
    ####    PyMOL_Mover is applied), when using a JobDistributor or otherwise
    ####    displaying multiple trajectories with a single protocol, the output
    ####    can get confusing to interpret, by changing the pose's PDBInfo.name
    ####    the structure will load into a new PyMOL state
    #### try uncommenting the lines below to see different output
    #pymover.update_energy(True)    # see the total score in color

    # 10. export the original structure, and scores, to PyMOL
    pymover.apply(pose)
    scorefxn(pose)
    pymover.send_energy(pose)

    # 11. setup a RepeatMover on a TrialMover of a SequenceMover (wow!)
    # -setup a TrialMover
    #    a. create a SequenceMover of the previous moves
    #### add any other moves you desire
    combined_mover = SequenceMover()
    combined_mover.add_mover(smallmover)
    combined_mover.add_mover(shearmover)
    combined_mover.add_mover(minmover)
    combined_mover.add_mover(packmover)
    #### explore the protocol using the PyMOL_Mover, try viewing structures
    ####    before they are accepted or rejected
    combined_mover.add_mover(pymover)
    #    b. create a MonteCarlo object to define success/failure
    mc = MonteCarlo(pose, scorefxn, kT)    # must reset for each trajectory!
    # c. create the TrialMover
    trial = TrialMover(combined_mover, mc)

    #### explore the protocol using the PyMOL_Mover, try viewing structures
    ####    after acceptance/rejection, comment-out the lines below
    #original_trial = TrialMover(combined_mover, mc)
    #trial = SequenceMover()
    #trial.add_mover(original_trial)
    #trial.add_mover(pymover)

    #### for each trajectory, try cycles number of applications

    # -create the RepeatMover
    refinement = RepeatMover(trial, cycles)
    ####

    # 12. create a (Py)JobDistributor
    jd = PyJobDistributor(job_output, jobs, scorefxn)
    jd.native_pose = starting_pose

    # 13. store the score evaluations for output
    # printing the scores as they are produced would be difficult to read,
    #    Rosetta produces a lot of verbose output when running
    scores = [0]*(jobs + 1)
    scores[0] = scorefxn(starting_pose)

    # 14. perform the refinement protocol
    counter = 0    # for exporting to PyMOL
    while not jd.job_complete:
        # a. set necessary variables for the new trajectory
        # -reload the starting pose
        pose.assign(starting_pose)
        # -change the pose's PDBInfo.name, for the PyMOL_Mover
        counter += 1
        pose.pdb_info().name(job_output + '_' + str(counter))
        # -reset the MonteCarlo object (sets lowest_score to that of p)
        mc.reset(pose)
        #### if you create a custom protocol, you may have additional
        ####    variables to reset, such as kT

        #### if you create a custom protocol, this section will most likely
        ####    change, many protocols exist as single Movers or can be
        ####    chained together in a sequence (see above) so you need
        ####    only apply the final Mover
        # b. apply the refinement protocol
        refinement.apply(pose)
        ####

        # c. output the lowest scoring decoy structure for this trajectory
        # -recover and output the decoy structure to a PDB file
        mc.recover_low(pose)
        jd.output_decoy(pose)
        # -export the final structure to PyMOL for each trajectory
        pose.pdb_info().name( job_output + '_' + str(counter) + '_final')
        pymover.apply(pose)
        pymover.send_energy(pose)    # see the total score in color

        # -store the final score for this trajectory
        scores[counter] = scorefxn(pose)

    # 15. output the score evaluations
    print 'Original Score\t:\t' , scores[0]
    for i in range(1, len(scores)):    # print out the job scores
        print job_output + '_' + str(i) + '\t:\t', scores[i]

    return scores    # for other protocols

################################################################################
# INTERPRETING RESULTS

"""
The (Py)JobDistributor will output the lowest scoring pose for each trajectory
(as a PDB file), recording the score in <job_output>.fasc. Generally,
the decoy generated with the lowest score contains the best prediction
for the protein conformation. Refinement (or relaxation) or some form of
"high-resolution refinement" is often part of a Rosetta protocol.
Although the input structure to a refinement process may represent a
likely guess for the protein structure, it can still conflict with the
Rosetta ScoreFunction. Refinement steps serve to improve the accuracy
of conformation predictions and to "smooth" the conformation within the
Rosetta scoring function landscape so various decoys (trajectory results)
can be compared effectively. This is also why refinement is effective for
"adapting" crystal structures to Rosetta. Refinement protocols should be
incapable of perturbing a protein conformation significantly however this may
still alter functionally important regions (such as loops). Individual
inspection of the conformations (such viewing in PyMOL) should accompany the
interpretation of results. Score comparison and visual inspection are critical
when testing out a new protocol. A refinement protocol should lower score
without altering the protein conformation significantly (typically RMSD<4).

The PyMOL_Mover speeds up investigation by directly loading structures into
PyMOL. This allows you to easily view intermediate changes in a protocol. In
the sample_refinement method, the original structure is exported to PyMOL and
colored by its per-residue score evaluation. For each trajectory, a series of
intermediate structures (proposed structures before Monte Carlo evaluation, see
above) are output with successive moves placed into successive states in PyMOL
(named <job_output>_(job#)). Cycle through these states (the "play" button in
the lower corner in the PyMOL viewer) to view a trajectory. The lowest scoring
structures from each trajectory are also exported to PyMOL and colored by
per-residue score (named <job_output>_(job#)_final). Score evaluations for each
trajectory are stored and output at the end of the protocol. This is a luxury
feature since the information is also stored in <job_output>.fasc. During a
single trajectory, the intermediates sent to PyMOL may be obviously incorrect.
These display how drastic of sampling is performed, if the structures are
frequently broken, you may want to adjust the protocol parameters to reduce
the search space.

"""

################################################################################
# COMMANDLINE COMPATIBILITY

# everything below is added to provide commandline usage,
#   the available options are specified below
# this method:
#    1. defines the available options
#    2. loads in the commandline or default values
#    3. calls sample_refinement with these values

# parser object for managing input options
# all defaults are for the example using "test_in.pdb" with reduced
#    cycles/jobs to provide results quickly
parser = optparse.OptionParser()
parser.add_option('--pdb_filename', dest = 'pdb_filename',
    default = '../test/data/test_in.pdb',    # default example PDB
    help = 'the PDB file containing the protein to refine')
# custom refinement options
parser.add_option('--kT', dest='kT',
    default = '1.0',
    help = 'the \"temperature\" of the sample refinement protocol')
parser.add_option( '--smallmoves', dest='smallmoves',
    default = '3',
    help = 'the number of times SmallMover is applies in\
        the custom refinement protocol' )
parser.add_option('--shearmoves', dest='shearmoves',
    default = '5',
    help = 'the number of times ShearMover is applied in\
        the custom refinement protocol' )
parser.add_option( '--backbone_angle_max', dest='backbone_angle_max',
    default = '7',
    help = 'the maximum angle perturbation of SmallMover and ShearMover in\
        the custom refinement protocol')
parser.add_option('--cycles', dest='cycles',
    default = '9',
    help = 'the number of refinement rounds (small, shear, min, pack) in\
        the sample refinement protocol')
# PyJobDistributor options
parser.add_option('--jobs', dest='jobs',
    default = '1',    # default to single trajectory for speed
    help = 'the number of jobs (trajectories) to perform')
parser.add_option('--job_output', dest = 'job_output',
    default = 'refine_output',    # if a specific output name is desired
    help = 'the name preceding all output, output PDB files and .fasc')
(options,args) = parser.parse_args()

# PDB file option
pdb_filename = options.pdb_filename
# custom refinement options
kT = float(options.kT)
smallmoves = int(options.smallmoves)
shearmoves = int(options.shearmoves)
backbone_angle_max = int(options.backbone_angle_max)
cycles = int(options.cycles)
# JobDistributor options
jobs = int(options.jobs)
job_output = options.job_output

sample_refinement(pdb_filename,
    kT, smallmoves, shearmoves, backbone_angle_max, cycles,
    jobs, job_output)

################################################################################
# ALTERNATE SCENARIOS

################
# A Real Example
"""
All of the default variables and parameters used above are specific to
the example with "test_in.pdb", which is supposed to be simple,
straightforward, and speedy. Here is a more practical example:

Several PDB files are not explicitly "Rosetta-friendly". This can occur for
numerous reasons but sometimes it is just a disagreement between the crystal
and Rosetta. Suppose you plan to use the FAF1 UBX domain for docking in Rosetta
but first want to ensure the PDB file is "Rosetta-friendly" by relaxing it.

1. Download a copy of RCSB PDB file 3R3M
2. Eliminate all HETATM lines and any ATOM lines not part of chain A,
        save it, lets name it "3R3M.clean.pdb" here
        (the crystal structure is a tetramer, we only care about the monomer)
3. Make a directory containing:
        -the PDB file "3R3M.clean.pdb"
        -this sample script (technically not required, but otherwise the
            commands in 4. would change since loops.py would not be here)
4. Run the script from the commandline with appropriate arguments:

>python refinement.py --pdb_file 3R3M.clean.pdb --jobs 60 --job_output 3R3M_refine_output --kT 0.8 --smallmoves 3 --shearmoves 5 --backbone_angle_max 10 --cycles 20

    -60 trajectories is low but it since the Metropolis Criteria is used and
        only the lowest scoring structure is output, the results greatly depend
        on the "temperature", kT, and the sampling method
    -kT is the "temperature" parameter and determines how high of a score is
        acceptable on a move, enabling the structure to escape local minima
        in the scoring "landscape"
    -the parameters smallmoves, shearmoves, and backbone_angle_max determine
        how drastic the sampling is (size of change), most changes will likely
        increase the score so kT must also be "tuned" to match these parameters
        or the pose will not change
    -the number of applications per trajectory, including backbone minimization,
        sidechain packing, and Metropolis assessment, this parameter directly
        affects the amount of sampling
    -the PyMOL_Mover_ip option is left to its default (127.0.0.1)
    -please consult the literature for more details on how to implement a
        more useful refinement method

5. Wait for output, this will take a while (20 rounds of minimization and
        sidechain packing on every residue for 60 trajectories)
6. Analyze the results (see INTERPRETING RESULTS above)

Note: this is NOT intended to be used for realistic refinement,
it merely provides a "skeleton" for the code in PyRosetta. It may be useful
for preliminary investigation but the best protocols are somewhat
protein-specific, there is no current universal refinement method. Generally,
a protocol similar to the one presented here with more drastic sampling and
a larger number of trials should be sufficient to find low scoring structures
near the starting conformation.

"""

##############################
# Changing Refinement Sampling
"""
All changes performed in this protocol correspond to specific Mover objects in
PyRosetta. This enables the user to combine all of these Movers into a single
SequenceMover and TrialMover, allowing (what appears to be) only a single
.apply call in the protocol. Refinement protocols will usually utilize some
form of torsion minimization and sidechain packing. Since these methods are
deterministic (or close to it), the actual sampling occurs before these Movers
are applied. Refinement protocols frequently employ small changes to the
backbone and sidechain torsions. This combination of moves does not drastically
alter the conformation of the protein and is thus considered to sample the
local conformation space around the structure to be refined. Refinement must
produce accurate results and is thus performed in fullatom mode.

The protocol here uses the SmallMover and ShearMover to produce small changes
and minimization plus sidechain packing to ensure a suitable score. Any
sampling protocol is potentially useful for exploring the fullatom conformation
space. Although centroid poses can save significant time, sampling is much
more drastic and refinement protocols should be restricted to fullatom

"""

#############################
# Changing Refinement Scoring
"""
The protocol here uses the Rosetta standard score function. The weights of this
function are optimized for use in various applications and may be somewhat
inappropriate for a custom protocol. As outlined in pose_scoring.py, you can
manually adjust (or turn on) scoring terms to create your custom score function.
Though non-standard, you can even create multiple score functions capturing
different constraints on the structure and pair these with individual MonteCarlo
and TrialMover objects. Depending on the selection criteria, this may
drastically reduce the chance of a permissible structure, and thus increase the
sampling required to acheive a desired sample. In many Monte Carlo processes,
the parameters effecting acceptance are often tuned to acheive a desired
fraction of accepted moves (in this case, the easiest parameter to modify
is kT).

If you are interested in the changes in individual score terms, you can remove
these values and write them as you please (see pose_scoring.py) or export them
to PyMOL for structure coloring. The PyMOL_Mover.send_energy method accepts an
optional argument specifying which score term to display.

Please try alternate scoring functions or unique selection methods to better
understand which scoring terms contribute to performance and find what
scoring best suites your problem.

"""

###############################
# Refinement in Other Protocols
"""
As mentioned elsewhere, general refinement protocols are necessary for many
Rosetta applications. General refinement may make raw crystal structures
acceptable to a Rosetta score function "landscape". Some protocols produce
"rough" structures which do not fit well into the Rosetta score function
landscape, such as centroid protocols which convert into fullatom. Refinement
is essential for removing artifacts and allowing structural comparison using
Rosetta scores. Tailored and optimized refinement steps often accompany
individual Rosetta protocols. Within the other sample scripts, there will be
custom high-resolution steps to refine the output.

"""
