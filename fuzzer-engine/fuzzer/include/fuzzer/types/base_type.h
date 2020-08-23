#ifndef BASE_TYPE_H
#define BASE_TYPE_H
template <typename T> class BaseType {
public:
  virtual ~BaseType(){};
  virtual T generate() = 0;


protected:
  BaseType(string varName, string varType)
      : varName(varName), varType(varType) {}
  string varName;
  string varType;
  T value;

private:
};
#endif // BASE_TYPE_H
