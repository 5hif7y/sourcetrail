// test.cc
// Test for implicit-dtors.exe.

class HasDtor {
public:
  // Note that the CFG omits calls to trivial destructors, so we have to
  // provide a definition here to get output.
  ~HasDtor() {}
};

class MemberHasDtor {
public:
  HasDtor m_hasDtor;

  // Implicit call to member ~HasDtor expected here.
  ~MemberHasDtor() {}
};

class InheritsHasDtor : public HasDtor {
public:
  // Implicit call to base class ~HasDtor expected here.
  ~InheritsHasDtor() {}
};

void callsTempObjDtor()
{
  // Implicit temporary object destructor.
  HasDtor();
}

void callsAutomaticObjDtor()
{
  // Implicit destructor of "automatic" object, which is different from
  // a temporary because the former arises when a local reference is
  // bound, and consequently its lifetime extended.
  HasDtor const &localReference = HasDtor();
}

void callsDeleteDtor()
{
  // Destructor due to 'delete'.
  delete (new HasDtor());
}

// EOF
