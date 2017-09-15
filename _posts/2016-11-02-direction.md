---
layout: post
title: Direction strategies
subtitle: 
#bigimg: /img/jupedsim_small.png
permalink: 2016-11-02-direction.html
---


The desired direction of a pedestrian is defined following different algorithms:
In the section of the chosen model the direction strategy should be specified as follows:

```xml
<exit_crossing_strategy>num</exit_crossing_strategy>
```

with *num* a positive integer.

The majority of the strategies define how a pedestrian crosses a line $$L = [P_1, P_2]$$. Possible values are:  

1. The direction of the pedestrian is towards the middle of $$L$$ ($$\frac{P_1+P2}{2}$$)  

2. The direction is given by the nearest point on $$L$$ to the position of the pedestrian.  
   $$L$$ is shorten by 20 cm.  

3. If the nearest point of the pedestrian on the segment line $$L$$ is outside the segment, then chose the middle point as target.  
   Otherwise the nearest point is chosen.

4. This strategy is still beta. It assumes that the simulation scenario has no loops or U-shaped corridors.
   Pedestrians,  target he exit, even if it is outside their visibility range. In case of intersection with walls or obstacles, the temporary direction is rotated away from the wall.  

5. Does not exist.

6. -9. Strategies using floor fields (ff) (vector fields); one ff per target (door, line, ...)

<img src="
https://cst.version.fz-juelich.de/jupedsim/jpscore/uploads/785cda284f5f44d2b019332d29b8075e/transformFF.png" width="300" height="300" />

  * 6: This strategy does use a floor field rather than heading towards a point on a line segment.

    For more details see this talk [^talk_arne] and the corresponding thesis [^thesis_arne].
     
    (__do not use in multi-storage buildings__)
    
  * 7: (__experimental__)
  
  * 8: This strategy uses a floor field collection for each room. 

    Thus the floor fields are smaller but cannot steer to targets in a different room. 

    The router __must__ provide intermediate targets for every agent, the target being in the same room.
    
    The projection of the room onto the ($$x,\, y$$)-plane must be non-overlapping!

  * 9: This strategy uses a floor field collection for each subroom. (__broken__)

    Thus the floor fields are again smaller but cannot steer to targets in a different subroom. 

    The router __must__ provide intermediate targets for every agent, that target being in the same subroom.
    
    The projection of the room onto the ($$x,\, y$$)-plane must be non-overlapping!

---   


[^talk_arne]: (https://fz-juelich.sciebo.de/index.php/s/s1ORGTUssCsHDHC)  
[^thesis_arne]: (https://fz-juelich.sciebo.de/index.php/s/VFnUCH2gtz1mSoL)  

    
[#Chraibi2011]: http://aimsciences.org/journals/displayPaper.jsp?paperID=6440 "Chraibi el al. Force-based models of pedestrian dynamics.  Pages: 425 - 442, Volume 6, Issue 3, September 2011"

