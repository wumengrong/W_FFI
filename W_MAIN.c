/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_W_FFI.h"
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>


static zend_object_handlers w_ffi_library_object_handlers;

//ZEND_DECLARE_MODULE_GLOBALS(W_FFI)
static zend_object_handlers w_ffi_object_handlers;

//初始化全局变量
/*static void php_W_FFI_init_globals(zend_W_FFI_globals *W_FFI_globals)
{
	W_FFI_globals->global_value = 0;
	W_FFI_globals->global_string = NULL;
}*/

///根据类型名称获得对应的数值,比如int 等价于1
long php_w_ffi_get_num_by_str(char*typeStr){
   long val=1;
   if(typeStr=="int")
   {
          val=1;
   }
   if(typeStr=="float")
   {
           val=2;
   }
   if(typeStr=="double")
   {
           val=3;
   }
   if(typeStr=="long")
   {
          val=4;
   }
   if(typeStr=="char")
   {
           val=5;
   }
   if(typeStr=="void*")
   {
           val=6;
   }
   if(typeStr=="char*")
   {
            val=7;
   }

   return val;
}
///根据数字返回对应的类型名称
char*php_w_ffi_get_str_by_num(long num){
    char*p=NULL;
    switch(num)
    {
        case 1:
            p="int";
            break;
        case 2:
            p="float";
            break;
        case 3:
            p="double";
            break;
        case 4:
            p="long";
            break;
        case 5:
            p="char";
            break;
        case 6:
            p="void*";
            break;
        case 7:
            p="char*";
        default:
            p="int";
    }
    return p;
}

//load library to memory
PHP_METHOD(W_MAIN,__construct)
{
   zend_string*libraryName=NULL;
   char path[50]={"/lib/test"};   //动态库目录
   char*postfix=".so";
   void *handle=NULL;

   php_w_ffi_object*obj;

 //  W_FFI_G(global_value); //获取全局变量global_value;
   if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"S",&libraryName)==FAILURE)
   {
         RETURN_NULL();
   }
   //getThis获得zval*指针，指针指向的区域存储了对象，在调用__construct开始就已经为对象分配了空间，__construct只是对对象进行赋值等操作
   //Z_OBJ_P(getThis());获得生成的对象的指针
   obj=php_w_ffi_fetch_object(Z_OBJ_P(getThis()));

   strcat(path,libraryName->val);
   strcat(path,postfix);
   if(libraryName!=NULL)
   {
   //handle=dlopen("/lib/test/score.so",RTLD_LAZY);
   //printf("sdfsdsdf");
     handle=dlopen(path,RTLD_LAZY);
   }
   else
   {
     handle=dlopen(NULL,RTLD_LAZY);
   }

   if(!handle)
   {
      printf("load %s dynamic library is fail",libraryName->val);
      printf("dlopen - %sn", dlerror());
      return;
   }
   //printf("handle%d",handle);
   obj->handle=handle;
   //printf("obj.handle%d",obj->handle);
}

///using hashTable store function pointer
PHP_METHOD(W_MAIN,bind)
{
   php_w_ffi_function_object *functionObj = NULL;

   zend_string*funcName=NULL;  //调用函数
   zend_array*reqPara=NULL;   //请求参数类型数组，存储用户将要调用函数的参数类型
   long retPara=0;          //返回参数类型，这是数字和类型的映射

   php_w_ffi_object*obj;
   void *handle = NULL;

   //h是hashtable类型的数组,l是long
   if(zend_parse_parameters(ZEND_NUM_ARGS(),"S|hl",&funcName,&reqPara,&retPara)==FAILURE)
    {
          RETURN_NULL();
    }

    obj=php_w_ffi_fetch_object(Z_OBJ_P(getThis()));
    if(!obj->handle)  //判断dlopen的结果是否存在
    {
         return;
    }

   //以function为zend_class_entry生成一个对象实例，将传递的参数赋值给对象实例，如返回值类型，参数类型，参数个数，dlsym句柄
    long reqParaType[10]={0}; //请求参数不超过10个，按照顺序将其类型存储在数组中
     ///获取传递过来的请求参数的类型
    int elementCount=0;
    if(reqPara!=NULL)
    {
         //reqPare is hashtable array
         ZEND_HASH_FOREACH(reqPara,0);
         reqParaType[elementCount]=_z->value.lval;
         elementCount++;
         ZEND_HASH_FOREACH_END();
    }

    object_init_ex(return_value,w_ffi_ce_function);
    functionObj = php_w_ffi_function_fetch_object(Z_OBJ_P(return_value));///获取对象
    handle=dlsym(obj->handle,funcName->val);
    if(!handle)
    {
      printf("handle is error");
      goto cleanup;
      return;
    }
    //存储dlsym句柄
    functionObj->function=handle;
    //存储返回值类型
     functionObj->php_return_type=retPara;  //,retPara是数字，需要将各类型与数值进行映射；
    //存储参数个数
    functionObj->arg_count=elementCount;
    //存储参数类型,是一数组,如果参数个数小于等于6时
    if(functionObj->arg_count<=6)
    {
     functionObj->php_arg_types=ecalloc(6,sizeof(long));
    }
    else
    {
    //如果参数个数大于6时
      functionObj->php_arg_types=ecalloc(functionObj->arg_count,sizeof(long));
    }

    long i=0;
    for(i=0;i<elementCount;i++)
    {
      functionObj->php_arg_types[i]=reqParaType[i];
    }
    return;

cleanup:
	efree(functionObj->php_arg_types);
	efree(functionObj);
	return;
}

php_w_ffi_object* php_w_ffi_fetch_object(zend_object*obj)
{
   //std是php_w_ffi_object中的属性，std is zend_object type,将对象的空间添加一个指针大小。
   return (php_w_ffi_object*)((char*)(obj) - XtOffsetOf(php_w_ffi_object,std));
}

static zend_object*w_ffi_object_new(zend_class_entry*ce)
{
    //php_w_ffi_object *object = ecalloc(1, sizeof(php_w_ffi_object) + zend_object_properties_size(ce));
    php_w_ffi_object *object = ecalloc(1, sizeof(php_w_ffi_object));
   // printf("sizeof%ld",sizeof(*object));
  	zend_object_std_init(&object->std, ce);             //对对象进行标准化的初始化，添加一些内核生成的内容
  	object_properties_init(&object->std, ce);          //对对象的属性进行初始化
  	object->std.handlers = &w_ffi_library_object_handlers;     //将对象的指针指向我们生成的对象。
  	return &object->std;                             //将处理后的对象返回给调用的上一层。
}

static void w_ffi_object_free(zend_object*obj)
{
  php_w_ffi_object *intern = php_w_ffi_fetch_object(obj);
    	if (!intern) {
    		return;
    	}
    	zend_object_std_dtor(&intern->std);      //调用对象的析构函数。
}

const zend_function_entry w_main_methods[] = {
 	PHP_ME(W_MAIN, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
 	PHP_ME(W_MAIN, bind, NULL,ZEND_ACC_PUBLIC)
 	PHP_FE_END
 };


PHP_MINIT_FUNCTION(W_MAIN)
{
    zend_class_entry main_ce;

    memcpy(&w_ffi_library_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    w_ffi_library_object_handlers.offset = XtOffsetOf(php_w_ffi_object, std);
    w_ffi_library_object_handlers.free_obj = w_ffi_object_free;
    w_ffi_library_object_handlers.clone_obj = NULL;

    INIT_NS_CLASS_ENTRY(main_ce,"W_FFI","Library",w_main_methods); //给当前类添加命名空间，同时添加类的方法
    main_ce.create_object=w_ffi_object_new;
    //将main_ce注册到内部中，同时会返回一个zend_class_entry*的指针，这个指针可以保留也可以不使用
    zend_register_internal_class(&main_ce);
   // w_main_library=zend_register_internal_class(&main_ce);
   //ZEND_INIT_MODULE_GLOBALS(W_FFI, php_W_FFI_init_globals, NULL);//将全局变量进行初始化
    return SUCCESS;
}


PHP_MSHUTDOWN_FUNCTION(W_MAIN)
{
   return SUCCESS;
}
/*
PHP_RINIT_FUNCTION(W_FFI)
{

	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(W_FFI)
{

	return SUCCESS;
}*/

/*
zend_module_entry W_FFI_module_entry = {
	STANDARD_MODULE_HEADER,
	"W_FFI",

	w_main_methods,
	PHP_MINIT(W_MAIN),
	PHP_MSHUTDOWN(W_MAIN),
	NULL,
	NULL,
	PHP_MINFO(W_MAIN),
	PHP_W_FFI_VERSION,
	STANDARD_MODULE_PROPERTIES
};*/
