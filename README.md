# GSRD
The Gray-Scott Reaction-Diffusion model - a discrete spatial implementation requires only moderate mathematical/programming ability and yet can yield intriguing visual patterns that resemble microbial growth. A more detailed descripton and discussion of this model may be found at:
   https://groups.csail.mit.edu/mac/projects/amorphous/GrayScott

---
Copyright: the project contributors, February-March 2018.

Background

Other than its aesthetic appeal, GSRD is of interest here precisely because of its simplicity compared to other RD models that attempt to simulate phenomena observable in the real-world. Such simulation models tend to be more complex in every respect: irregular spatial structure in 3D, many mobile reacting components, incorporation of phases with variable pressure and temperature, flow in addition to diffusive transport, and far more complex systems of coupled partial differential equations. As an example of a higher complexity model, consider microbial activity within porous media having to an effect on wet-dry cycle behaviour within pore space at small length scales (tens of millimetres or less). Such behaviour potentially leads to large scale phenomena affecting e.g the drainage versus retention of surface water. An improved understanding of such phenomena may be able to better inform policy making for more effective management within the natural and built environment.

A major barrier to the developemnt of effective simulation models is their computational complexity - it takes a great deal of processing power to simulate enough replicates at an appropriate spatial scale such that reliable conclusions can be drawn. Optimising the computation is thus highly desireable in order to obtain more results with less resources (time, energy and hardware). Yet this optimisation process intrinsically relies on running many replicates (software variants) of the model in order to make intelligent decisions regarding optimisation strategy. Consequently an initial investigation using a simplified model is attractive: less effective strategies can be rapidly eliminated, allowing effort to be concentrated on the most promising optimisations.


Software Development - General Strategy

1) Develop prototype in Python/NumPy/SciPy - ideal for small scale rough experiments and general data wrangling.
2) Develop version in C to achieve better performance - handling boundary conditions leads to lengthy code.
3) Parallelise C version for CPU/GPU using OpenACC - NVIDIA/PGI compiler directives generate SSE/CUDA code.
4) Distribute workload across multiple CPU/GPU devices within a single host to improve throughput - MPI might be used.
5) If feasible, consider distribution across multiple hosts (cluster) using MPI.


Notes

OpenACC/CUDA has been problematic to get working properly. An old MSI laptop was set up as a headless server named INKCAP running Centos7 OS. Ubuntu and Debian OS were also considered (and investigated to a limited extent) but judged to be more problematic to set up with necessary GPU device drivers. In any case, considerable difficulty was encountered in getting a suitable video (GPU) driver working. After low-level fiddling (blacklisting Nouveau driver on boot) and numerous failed attempts at installing "recommended" drivers from package archives, success was realised by using the recently released "NVIDIA-Linux-x86_64-390.25.run" installer obtained from the NVidia driver download site. Suddenly everything is working correctly at last.

PARTIAL-DEPRECATION{
Acceleration using the PGI compiler seems to be determined at build time only by specifying relevant options. Furthermore, if GPU acceleration is enabled within a build then this appears to supersede any host (e.g. multi-core) acceleration options. Initialisation calls within the code can apparently be used to select a specific GPU device, hence tasks might be apportioned between a set of devices. If differing acceleration options are desired for various devices within a single build, then some code duplication seems to be necessary. This might be simplified in some cases by using macro substitution on included source code fragments (perhaps at the level of individual functions). Thus acceleration directives could be customised for instances while retaining a common "backbone" algorithm implementation that may simplify some aspects of development and maintenance.
}
The previous paragraph is not entirely true! If accelerated code is compiled with suitable target options eg. "-ta=multicore,tesla=cc50" then using the function "acc_set_device_num()" allows selection of a specific device compatible with one of those target options. However if different parallelisation strategies are appropiate for different devices, then multi-compilation with macro-substitution may still be necessary...

A simple 1D diffusion test was created to test OpenACC parallelisation; this proved to be much simpler to follow and clarified some confusing errors. In particular, it seems necessary that any dynamically allocated buffer used for GPU acceleration must  be referenced only by the original address returned from malloc() i.e. it is not possible to divide a single huge buffer into individually referenced sub-buffers (offsets within a huge buffer could be used to achieve the same effect however, at the expense of code clarity). Presumably the OpenACC/CUDA runtime is hooking into dynamic allocations (either via the CRT library or some OS feature) to create/verify address mapping/aliasing between host and device address spaces. The consequence - allocating individual buffers - makes init/shutdown code a bit uglier, and the inability to assume sub-buffer contiguity is slightly inconvenient, but otherwise this is the clearer solution.

The 1D diffusion test produces the same effective result on CPU versus GPU - in the latter case rounding of "infinitessimal" values to zero seems not to occur, meaning that exact comparison of the methods will signal failure. Approximate comparison using an epsilon value resolves this issue. 

Following some CUDA driver issues, OpenACC is now working on the multi-GPU compute server running Ubuntu 16.04LTS. Installing NVidia driver 390.25 from an apt repository (after first disabling lightdm, then re-enabling after reboot) did the trick. The <pgaccelinfo> command now lists three GTX1080 devices compatible with CUDA9.1 (-ta=tesla:cc60).




---
