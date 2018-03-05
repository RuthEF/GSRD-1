# GSRD
The Gray-Scott Reaction-Diffusion model - a discrete spatial implementation requires only moderate mathematical/programming ability and yet can yield intriguing visual patterns that resemble microbial growth. A more detailed descripton and discussion of this model may be found at:
   https://groups.csail.mit.edu/mac/projects/amorphous/GrayScott

---
Copyright: the project contributors, February-March 2018.

Background

Other than its aesthetic appeal, GSRD is of interest here precisely because of its simplicity compared to other RD models that attempt to simulate phenomena observable in the real-world. Such simulation models tend to be more complex in every respect: irregular spatial structure in 3D, many mobile reacting components, incorporation of phases with variable pressure and temperature, flow in addition to diffusive transport, and far more complex systems of coupled partial differential equations. An example of a higher complexity model could be the microbial activity within porous media leading to an effect on wet-dry cycle behaviour. Potentially such effects may have some large scale effect on drainage versus retention of surface water. An improved understanding of such phenomena may be able to better inform policy making for more effective management within the natural and built environment.
A major barrier to the developemnt of effective simulation models is their computational complexity - it takes a great deal of processing power to simulate enough replicates at an appropriate spatial scale such that reliable conclusions can be drawn. Optimising the computation is thus highly desireable in order to obtain more results with less resources (time, energy and hardware). Yet this optimisation process intrinsically relies on running many replicates (software variants) of the model in order to make intelligent decisions regarding optimisation strategy. Consequently an initial investigation using a simplified model is attractive: less effective strategies can be rapidly eliminated, allowing effort to be concentrated on the most promising optimisations.


Software Development - General Strategy

1) Develop prototype in Python/NumPy/SciPy - ideal for small scale rough experiments and general data wrangling.
2) Develop version in C to achieve better performance - handling boundary conditions leads to lengthy code.
3) Parallelise C version for CPU/GPU using OpenACC - NVIDIA/PGI compiler directives generate SSE/CUDA code.
4) Distribute workload across multiple CPU/GPU devices within a single host to improve throughput - MPI might be used.
5) If feasible, consider distribution across multiple hosts (cluster) using MPI.


Notes

OpenACC/CUDA has been problematic to get working properly. An old MSI laptop was set up as a headless server named INKCAP running Centos7 OS. Ubuntu and Debian OS were also considered (and investigated to a limited extent) but judged to be more problematic to set up with necessary GPU device drivers. In any case, considerable difficulty was encountered in getting a suitable video (GPU) driver working. After low-level fiddling (blacklisting Nouveau driver on boot) and numerous failed attempts at installing "recommended" drivers from package archives, success was realised by using the recently released "NVIDIA-Linux-x86_64-390.25.run" installer obtained from the NVidia driver download site. Suddenly everything is working correctly at last.

---