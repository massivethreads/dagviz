using namespace std;

/* XAxis */
typedef struct XAxis {
  struct XAxis * l, r;
  int level;
  
  VNode 
} XAxis;

/* YAxis */
typedef struct YAxis {
  struct YAxis * l, r;
  int level;
} YAxis;

/* VNode */
typedef struct VNode {
  int type; // 0:com, 1:unified, 2:expanded
  
} VNode;

/* View */
class View
{
 public:
  View();
  virtual ~View();
  void scan_model(Model model);

 private:
  XAxis xaxis;
}
