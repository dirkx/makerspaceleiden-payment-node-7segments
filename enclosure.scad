$fn=60;

// Catalex 7 segement display
QUAD7_W=42;
QUAD7_H=24;
QUAD7_T=1;

QUAD7_DW=30;
QUAD7_DH=14;
QUAD7_DT=8;

QUAD7_HD=2;
QUAD7_HW=37.5;
QUAD7_HH=19.5;
QUAD7_HO=QUAD7_HD/2+1;
QUAD7_H_ASSYM=1;

showdev=0;
studs=0;
version="v1.03";
/*
   v1.01   white print 15/10/2021 for box with transparant cover.
   v1.02   scanner 1mm lower, whole plate 2mm lower, rfid pillars narrower
           pins display 0.5 mm to the inside in all directions. display
           display mm shifted to right for connector assymetry. text deeper
           and bolder.
   v1.03   Standoffs 2mm shorter.
*/
module quad_holes(r=QUAD7_HD/2,h=10,top=1) {
   r2=r; r1=r*top;
   translate([QUAD7_HO,QUAD7_HO,-0.01]) cylinder(r2=r2,r1=r1,h=h);
   translate([QUAD7_W-QUAD7_HO,QUAD7_HO,-0.01]) cylinder(r2=r2,r1=r1,h=h);
   translate([QUAD7_W-QUAD7_HO,QUAD7_H-QUAD7_HO,-0.01]) cylinder(r2=r2,r1=r1,h=h);
   translate([QUAD7_HO,QUAD7_H-QUAD7_HO,-0.01]) cylinder(r2=r2,r1=r1,h=h);
};

module quad() {
   union() {
       difference() {
           color([0,0,0.6]) cube([QUAD7_W,QUAD7_H,QUAD7_T]); 
           quad_holes();
           }
       translate([(QUAD7_W-QUAD7_DW)/2, (QUAD7_H-QUAD7_DH)/2,QUAD7_T]) 
               color([1,1,1]) cube([QUAD7_DW,QUAD7_DH,QUAD7_DT]);
       translate([(QUAD7_W-QUAD7_DW)/2, (QUAD7_H-QUAD7_DH)/2,QUAD7_T+QUAD7_DT]) 
               color([0,0,0]) cube([QUAD7_DW,QUAD7_DH,0.01]);
   };
};


module display(h=10) {
translate([-QUAD7_W/2+QUAD7_H_ASSYM,-QUAD7_H/2,-(QUAD7_T+QUAD7_DT)]) {
   if (showdev>0) quad();
   translate([0,0,-h+4]) quad_holes(QUAD7_HD/2,h);
   translate([0,0,-h]) quad_holes(QUAD7_HD/2+0.5,h,1.6);
};
};

RC_H=59.5;
RC_W=39.5;
RC_T=1;

RC_HD=3.2;
RC_HW1=24.5; // top
RC_HW2=34.0; // near connector
RC_HH12=37.0; // between conectors.
RC_HT=5.0; // center to the top..

module rc_holes(r=RC_HD/2,h=10,top=1) {
   r2=r; r1=r*top;
   translate([(RC_W-RC_HW1)/2,RC_H-RC_HT,-0.01])
       cylinder(r2=r2,r1=r1,h=h);
   translate([(RC_W+RC_HW1)/2,RC_H-RC_HT,-0.01])
       cylinder(r2=r2,r1=r1,h=h);
   translate([(RC_W-RC_HW2)/2,RC_H-RC_HT-RC_HH12,-0.01])
       cylinder(r2=r2,r1=r1,h=h);
   translate([(RC_W+RC_HW2)/2,RC_H-RC_HT-RC_HH12,-0.01])
       cylinder(r2=r2,r1=r1,h=h);
};

module rc() {
   union() {
       difference() {
           color([0,0,0.6]) cube([RC_W,RC_H,RC_T]); 
           rc_holes();
           }
   };
};

module rfidall(h=10) {
translate([-RC_W/2,-RC_H/2,-RC_T]) {
   if (showdev>0) rc();
   translate([0,0,-h+4]) rc_holes(RC_HD/2,h);
   translate([0,0,-h]) rc_holes(RC_HD/2+0.5,h,1.25);
};
};


WR_H=52;    // WROOM_DEVKIT 1.1
WR_H2=56;   // ++ version
WR_W=28.5;
WR_HD=2.75;
WR_T=3;
WR_HO=(WR_W-23)/2;

module wroom_holes(r=WR_HD/2,h=10) {
   translate([WR_HO,WR_HO,-0.01]) cylinder(r=r,h=h);
   translate([WR_W-WR_HO,WR_HO,-0.01]) cylinder(r=r,h=h);
   translate([WR_W-WR_HO,WR_H-WR_HO,-0.01]) cylinder(r=r,h=h);
   translate([WR_HO,WR_H-WR_HO,-0.01]) cylinder(r=r,h=h);
   // for the large version
   translate([WR_W-WR_HO,WR_H2-WR_HO,-0.01]) cylinder(r=r,h=h);
   translate([WR_HO,WR_H2-WR_HO,-0.01]) cylinder(r=r,h=h);
};
module wroom() {    
   union() {
       difference() {
           color([0,0,0.6]) cube([WR_W,WR_H,WR_T]); 
           wroom_holes();
           }
   };
};
module wroom_all(h=10) {
translate([-WR_W/2,-WR_H/2,-WR_T]) {
   if (showdev>0) wroom();
   translate([0,0,-h+4]) wroom_holes(WR_HD/2,h);
   translate([0,0,-h]) wroom_holes(WR_HD/2+0.5,h);
};
};


MB_W=61;
MB_H=91;
MB_T=2;
MB_P=45;
MB_V=80;
MB_HH=2;
MB_I=10;
MB_STUD=6.5;

module mb_pins(r=MB_HH/2,h=MB_T+0.01) {
   translate([(MB_W-MB_P)/2,(MB_H-MB_P)/2,0]) {
       translate([0,0,-0.01]) cylinder(r=r,h=h);
       translate([MB_P,0,-0.01]) cylinder(r=r,h=h);
       translate([MB_P,MB_P,-0.01]) cylinder(r=r,h=h);
       translate([0,MB_P,-0.01]) cylinder(r=r,h=h);
   };
   translate([(MB_W)/2,(MB_H-MB_V)/2,0]) {
       translate([0,0,-0.01]) cylinder(r=r,h=h);
       translate([0,MB_V,-0.01]) cylinder(r=r,h=h);
   };
};

module mb(h=10) {
   difference() {
       union() {
       cube([MB_W,MB_H,MB_T]);
       if (studs) translate([0,0,-h]) mb_pins(MB_STUD/2, h=h);
       }
       translate([0,0,-0.1]) cylinder(r=MB_I,h=MB_T+1);
       translate([MB_W,0,-0.1]) cylinder(r=MB_I,h=MB_T+1);
       translate([MB_W,MB_H,-0.1]) cylinder(r=MB_I,h=MB_T+1);
       translate([0,MB_H,-0.01]) cylinder(r=MB_I,h=MB_T+1);
       // drill holes
       translate([0,0,-h-2]) mb_pins(MB_HH/2, h=h+5);
       // wire hole
       translate([MB_W/2,MB_H-15,-0.01]) cylinder(r=5,h=3);
       // tiewrap holes
       translate([MB_W/2-5,MB_H-40,-0.01]) cylinder(r=2,h=3);
       translate([MB_W/2+5,MB_H-40,-0.01]) cylinder(r=2,h=3);
       translate([MB_W/2,MB_H-30,-0.4+MB_T]) 
           linear_extrude(height = 1)
           text(version,halign="center", valign="center",size=8,font="Gill Sans:style=Bold");
       translate([MB_W/2,MB_H-53,-0.4+MB_T]) 
           linear_extrude(height = 1)
           text("makerspaceleiden.nl",halign="center", valign="center",size=3,font="Gill Sans:style=Bold");
       translate([MB_W/2,MB_H-58,-0.4+MB_T]) 
           linear_extrude(height = 1)
           text("2021-10-15 / paymentnode",halign="center", valign="center",size=2.5, font="Gill Sans:style=Bold");
};
if (studs == 0) {
   translate([-70,0,0]) {
   difference() {
    mb_pins(MB_STUD/2, h=h);
    translate([0,0,-5]) mb_pins(MB_HH/2, h=h+10);
   };
};
};
};

module mountingboard(h=10) {
   translate([-MB_W/2,-MB_H/2,0]) {
   mb(h);
   // translate([0,0,-h+4]) mb_pins(WR_HD/2,h);
   // translate([0,0,-h]) mb_pins(WR_HD/2+0.5,h);
       };
};

PH=14; // package height/highest board.
IH=40; // insighed height

translate([0,0,IH]) 
   display(PH-3); 

translate([0,40,IH-QUAD7_DT+3]) 
   rfidall(PH+QUAD7_DT-8);

translate([0,9,IH-12]) 
   wroom_all(PH-9);

translate([0,28,IH-PH-8]) 
   mountingboard(IH-PH-8);

if (showdev)
   translate([-2,-22,0]) cube([5,5,IH]);    

