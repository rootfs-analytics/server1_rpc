#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>

struct Base0 : public boost::function1<void, boost::function0<void> >, boost::enable_shared_from_this<Base0> {
  virtual void Run() {
    printf("run base0");
  }
  virtual ~Base0() {
    printf ("~Base0");
  }
  Base0() {
    printf("Base0");
  }
};

template <typename A>
struct Base1 : public Base0, public boost::enable_shared_from_this<Base1<A> > {
  ~Base1() {
    printf ("~Base1");
  }
  Base1() {
    printf("Base1");
  }
  void Run() {
    printf ("run base1");
  }
};
struct Base3 : public boost::enable_shared_from_this<Base3> {
  ~Base3() {
    printf ("~Base3");
  }
  Base3() {
    printf("Base3");
  }
  void Run() {
    printf("run base3");
  }
};


template <typename C>
struct Base2 : private Base1<C>, public boost::enable_shared_from_this<Base2<C> > {
  ~Base2() {
    printf ("~Base2");
  }
  Base2() {
    printf("Base2");
  }
  void Run() {
    printf("run base2");
  }
  Base3 b3;
};

int main(int argc, char **argv) {
  /*
  boost::shared_ptr<Base2<int> > b2(new Base2<int>);
  b2->Base1<int>::Run();
  b2->Base2<int>::shared_from_this()->Run();
  b2->Base1<int>::shared_from_this()->Run();
  boost::shared_ptr<boost::function0<void> > b1(new boost::function0<void>(boost::bind(
      &Base2<int>::Run, b2->Base1<int>::shared_from_this())));
  b2->operator()();
  boost::shared_ptr<Base0> b0(new Base0);
  boost::shared_ptr<const boost::function0<void> > a(
      new boost::function0<void>(boost::bind(&Base0::Run, b0->shared_from_this())));
  b0->Run();
  */
  boost::scoped_ptr<Base2<int> > b2(new Base2<int>);
  Base2<int> * b22 = b2.get();
  b2.reset();
  b22->Run();
}
