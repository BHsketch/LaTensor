; ModuleID = 'matmul.cpp'
source_filename = "matmul.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

module asm ".globl _ZSt21ios_base_library_initv"

%"class.std::basic_ostream" = type { ptr, %"class.std::basic_ios" }
%"class.std::basic_ios" = type { %"class.std::ios_base", ptr, i8, i8, ptr, ptr, ptr, ptr }
%"class.std::ios_base" = type { ptr, i64, i64, i32, i32, i32, ptr, %"struct.std::ios_base::_Words", [8 x %"struct.std::ios_base::_Words"], i32, ptr, %"class.std::locale" }
%"struct.std::ios_base::_Words" = type { ptr, i64 }
%"class.std::locale" = type { ptr }
%"class.std::mersenne_twister_engine" = type { [624 x i64], i64 }

@_ZSt4cerr = external global %"class.std::basic_ostream", align 8
@.str = private unnamed_addr constant [19 x i8] c"Allocation failed\0A\00", align 1

; Function Attrs: mustprogress norecurse uwtable
define dso_local noundef i32 @main(i32 noundef %0, ptr nocapture noundef readnone %1) local_unnamed_addr #0 {
  %3 = alloca ptr, align 8
  %4 = alloca ptr, align 8
  %5 = alloca ptr, align 8
  %6 = alloca %"class.std::mersenne_twister_engine", align 8
  call void @llvm.lifetime.start.p0(i64 5000, ptr nonnull %6) #11
  store i64 42, ptr %6, align 8, !tbaa !5
  %7 = getelementptr inbounds nuw i8, ptr %6, i64 8
  br label %611

8:                                                ; preds = %611
  %9 = getelementptr inbounds nuw i8, ptr %6, i64 4992
  store i64 624, ptr %9, align 8, !tbaa !9
  call void @llvm.lifetime.start.p0(i64 8, ptr nonnull %5) #11
  store ptr null, ptr %5, align 8, !tbaa !11
  %10 = call i32 @posix_memalign(ptr noundef nonnull %5, i64 noundef 64, i64 noundef 65536) #11
  %11 = icmp eq i32 %10, 0
  br i1 %11, label %14, label %12

12:                                               ; preds = %8
  %13 = call noundef nonnull align 8 dereferenceable(8) ptr @_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc(ptr noundef nonnull align 8 dereferenceable(8) @_ZSt4cerr, ptr noundef nonnull @.str)
  call void @exit(i32 noundef 1) #12
  unreachable

14:                                               ; preds = %8
  %15 = load ptr, ptr %5, align 8, !tbaa !11
  call void @llvm.lifetime.end.p0(i64 8, ptr nonnull %5) #11
  call void @llvm.lifetime.start.p0(i64 8, ptr nonnull %4) #11
  store ptr null, ptr %4, align 8, !tbaa !11
  %16 = call i32 @posix_memalign(ptr noundef nonnull %4, i64 noundef 64, i64 noundef 65536) #11
  %17 = icmp eq i32 %16, 0
  br i1 %17, label %20, label %18

18:                                               ; preds = %14
  %19 = call noundef nonnull align 8 dereferenceable(8) ptr @_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc(ptr noundef nonnull align 8 dereferenceable(8) @_ZSt4cerr, ptr noundef nonnull @.str)
  call void @exit(i32 noundef 1) #12
  unreachable

20:                                               ; preds = %14
  %21 = load ptr, ptr %4, align 8, !tbaa !11
  call void @llvm.lifetime.end.p0(i64 8, ptr nonnull %4) #11
  call void @llvm.lifetime.start.p0(i64 8, ptr nonnull %3) #11
  store ptr null, ptr %3, align 8, !tbaa !11
  %22 = call i32 @posix_memalign(ptr noundef nonnull %3, i64 noundef 64, i64 noundef 65536) #11
  %23 = icmp eq i32 %22, 0
  br i1 %23, label %26, label %24

24:                                               ; preds = %20
  %25 = call noundef nonnull align 8 dereferenceable(8) ptr @_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc(ptr noundef nonnull align 8 dereferenceable(8) @_ZSt4cerr, ptr noundef nonnull @.str)
  call void @exit(i32 noundef 1) #12
  unreachable

26:                                               ; preds = %20
  %27 = load ptr, ptr %3, align 8, !tbaa !11
  call void @llvm.lifetime.end.p0(i64 8, ptr nonnull %3) #11
  call fastcc void @_ZL11fill_randomPfiRSt23mersenne_twister_engineImLm32ELm624ELm397ELm31ELm2567483615ELm11ELm4294967295ELm7ELm2636928640ELm15ELm4022730752ELm18ELm1812433253EE(ptr noundef %15, ptr noundef nonnull align 8 dereferenceable(5000) %6)
  call fastcc void @_ZL11fill_randomPfiRSt23mersenne_twister_engineImLm32ELm624ELm397ELm31ELm2567483615ELm11ELm4294967295ELm7ELm2636928640ELm15ELm4022730752ELm18ELm1812433253EE(ptr noundef %21, ptr noundef nonnull align 8 dereferenceable(5000) %6)
  %28 = getelementptr i8, ptr %15, i64 65536
  %29 = icmp ule ptr %28, %27
  %30 = getelementptr i8, ptr %27, i64 65536
  %31 = icmp ule ptr %30, %15
  %32 = or i1 %29, %31
  %33 = getelementptr i8, ptr %21, i64 65536
  %34 = icmp ule ptr %33, %27
  %35 = icmp ule ptr %30, %21
  %36 = or i1 %34, %35
  %37 = and i1 %32, %36
  br i1 %37, label %38, label %70

38:                                               ; preds = %26
  %39 = getelementptr i8, ptr %27, i64 512
  %40 = getelementptr i8, ptr %27, i64 1024
  %41 = getelementptr i8, ptr %27, i64 1536
  %42 = getelementptr i8, ptr %27, i64 2048
  %43 = getelementptr i8, ptr %27, i64 2560
  %44 = getelementptr i8, ptr %27, i64 3072
  %45 = getelementptr i8, ptr %27, i64 3584
  %46 = getelementptr i8, ptr %27, i64 4096
  %47 = getelementptr i8, ptr %27, i64 4608
  %48 = getelementptr i8, ptr %27, i64 5120
  %49 = getelementptr i8, ptr %27, i64 5632
  %50 = getelementptr i8, ptr %27, i64 6144
  %51 = getelementptr i8, ptr %27, i64 6656
  %52 = getelementptr i8, ptr %27, i64 7168
  %53 = getelementptr i8, ptr %27, i64 7680
  %54 = getelementptr i8, ptr %27, i64 8192
  %55 = getelementptr i8, ptr %27, i64 8704
  %56 = getelementptr i8, ptr %27, i64 9216
  %57 = getelementptr i8, ptr %27, i64 9728
  %58 = getelementptr i8, ptr %27, i64 10240
  %59 = getelementptr i8, ptr %27, i64 10752
  %60 = getelementptr i8, ptr %27, i64 11264
  %61 = getelementptr i8, ptr %27, i64 11776
  %62 = getelementptr i8, ptr %27, i64 12288
  %63 = getelementptr i8, ptr %27, i64 12800
  %64 = getelementptr i8, ptr %27, i64 13312
  %65 = getelementptr i8, ptr %27, i64 13824
  %66 = getelementptr i8, ptr %27, i64 14336
  %67 = getelementptr i8, ptr %27, i64 14848
  %68 = getelementptr i8, ptr %27, i64 15360
  %69 = getelementptr i8, ptr %27, i64 15872
  br label %118

70:                                               ; preds = %26, %78
  %71 = phi i64 [ %79, %78 ], [ 0, %26 ]
  %72 = shl nuw nsw i64 %71, 7
  %73 = getelementptr inbounds nuw float, ptr %15, i64 %72
  %74 = getelementptr inbounds nuw float, ptr %27, i64 %72
  br label %75

75:                                               ; preds = %81, %70
  %76 = phi i64 [ 0, %70 ], [ %83, %81 ]
  %77 = getelementptr inbounds nuw float, ptr %21, i64 %76
  br label %85

78:                                               ; preds = %81
  %79 = add nuw nsw i64 %71, 1
  %80 = icmp eq i64 %79, 128
  br i1 %80, label %117, label %70, !llvm.loop !14

81:                                               ; preds = %85
  %82 = getelementptr inbounds nuw float, ptr %74, i64 %76
  store float %114, ptr %82, align 4, !tbaa !17
  %83 = add nuw nsw i64 %76, 1
  %84 = icmp eq i64 %83, 128
  br i1 %84, label %78, label %75, !llvm.loop !19

85:                                               ; preds = %85, %75
  %86 = phi i64 [ 0, %75 ], [ %115, %85 ]
  %87 = phi float [ 0.000000e+00, %75 ], [ %114, %85 ]
  %88 = getelementptr inbounds nuw float, ptr %73, i64 %86
  %89 = load float, ptr %88, align 4, !tbaa !17
  %90 = shl nsw i64 %86, 9
  %91 = getelementptr inbounds nuw i8, ptr %77, i64 %90
  %92 = load float, ptr %91, align 4, !tbaa !17
  %93 = call float @llvm.fmuladd.f32(float %89, float %92, float %87)
  %94 = or disjoint i64 %86, 1
  %95 = getelementptr inbounds nuw float, ptr %73, i64 %94
  %96 = load float, ptr %95, align 4, !tbaa !17
  %97 = shl nsw i64 %94, 9
  %98 = getelementptr inbounds nuw i8, ptr %77, i64 %97
  %99 = load float, ptr %98, align 4, !tbaa !17
  %100 = call float @llvm.fmuladd.f32(float %96, float %99, float %93)
  %101 = or disjoint i64 %86, 2
  %102 = getelementptr inbounds nuw float, ptr %73, i64 %101
  %103 = load float, ptr %102, align 4, !tbaa !17
  %104 = shl nsw i64 %101, 9
  %105 = getelementptr inbounds nuw i8, ptr %77, i64 %104
  %106 = load float, ptr %105, align 4, !tbaa !17
  %107 = call float @llvm.fmuladd.f32(float %103, float %106, float %100)
  %108 = or disjoint i64 %86, 3
  %109 = getelementptr inbounds nuw float, ptr %73, i64 %108
  %110 = load float, ptr %109, align 4, !tbaa !17
  %111 = shl nsw i64 %108, 9
  %112 = getelementptr inbounds nuw i8, ptr %77, i64 %111
  %113 = load float, ptr %112, align 4, !tbaa !17
  %114 = call float @llvm.fmuladd.f32(float %110, float %113, float %107)
  %115 = add nuw nsw i64 %86, 4
  %116 = icmp eq i64 %115, 128
  br i1 %116, label %81, label %85, !llvm.loop !20

117:                                              ; preds = %78, %254
  call void @free(ptr noundef nonnull %15) #11
  call void @free(ptr noundef nonnull %21) #11
  call void @free(ptr noundef nonnull %27) #11
  call void @llvm.lifetime.end.p0(i64 5000, ptr nonnull %6) #11
  ret i32 0

118:                                              ; preds = %38, %118
  %119 = phi i64 [ %252, %118 ], [ 0, %38 ]
  %120 = shl nuw nsw i64 %119, 14
  %121 = getelementptr i8, ptr %27, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %121, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %122 = getelementptr i8, ptr %39, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %122, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %123 = getelementptr i8, ptr %40, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %123, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %124 = getelementptr i8, ptr %41, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %124, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %125 = getelementptr i8, ptr %42, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %125, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %126 = getelementptr i8, ptr %43, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %126, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %127 = getelementptr i8, ptr %44, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %127, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %128 = getelementptr i8, ptr %45, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %128, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %129 = getelementptr i8, ptr %46, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %129, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %130 = getelementptr i8, ptr %47, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %130, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %131 = getelementptr i8, ptr %48, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %131, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %132 = getelementptr i8, ptr %49, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %132, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %133 = getelementptr i8, ptr %50, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %133, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %134 = getelementptr i8, ptr %51, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %134, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %135 = getelementptr i8, ptr %52, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %135, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %136 = getelementptr i8, ptr %53, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %136, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %137 = getelementptr i8, ptr %54, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %137, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %138 = getelementptr i8, ptr %55, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %138, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %139 = getelementptr i8, ptr %56, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %139, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %140 = getelementptr i8, ptr %57, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %140, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %141 = getelementptr i8, ptr %58, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %141, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %142 = getelementptr i8, ptr %59, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %142, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %143 = getelementptr i8, ptr %60, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %143, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %144 = getelementptr i8, ptr %61, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %144, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %145 = getelementptr i8, ptr %62, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %145, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %146 = getelementptr i8, ptr %63, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %146, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %147 = getelementptr i8, ptr %64, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %147, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %148 = getelementptr i8, ptr %65, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %148, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %149 = getelementptr i8, ptr %66, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %149, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %150 = getelementptr i8, ptr %67, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %150, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %151 = getelementptr i8, ptr %68, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %151, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %152 = getelementptr i8, ptr %69, i64 %120
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %152, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %153 = or disjoint i64 %120, 128
  %154 = getelementptr i8, ptr %27, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %154, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %155 = getelementptr i8, ptr %39, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %155, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %156 = getelementptr i8, ptr %40, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %156, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %157 = getelementptr i8, ptr %41, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %157, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %158 = getelementptr i8, ptr %42, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %158, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %159 = getelementptr i8, ptr %43, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %159, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %160 = getelementptr i8, ptr %44, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %160, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %161 = getelementptr i8, ptr %45, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %161, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %162 = getelementptr i8, ptr %46, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %162, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %163 = getelementptr i8, ptr %47, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %163, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %164 = getelementptr i8, ptr %48, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %164, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %165 = getelementptr i8, ptr %49, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %165, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %166 = getelementptr i8, ptr %50, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %166, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %167 = getelementptr i8, ptr %51, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %167, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %168 = getelementptr i8, ptr %52, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %168, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %169 = getelementptr i8, ptr %53, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %169, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %170 = getelementptr i8, ptr %54, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %170, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %171 = getelementptr i8, ptr %55, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %171, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %172 = getelementptr i8, ptr %56, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %172, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %173 = getelementptr i8, ptr %57, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %173, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %174 = getelementptr i8, ptr %58, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %174, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %175 = getelementptr i8, ptr %59, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %175, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %176 = getelementptr i8, ptr %60, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %176, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %177 = getelementptr i8, ptr %61, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %177, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %178 = getelementptr i8, ptr %62, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %178, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %179 = getelementptr i8, ptr %63, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %179, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %180 = getelementptr i8, ptr %64, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %180, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %181 = getelementptr i8, ptr %65, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %181, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %182 = getelementptr i8, ptr %66, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %182, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %183 = getelementptr i8, ptr %67, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %183, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %184 = getelementptr i8, ptr %68, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %184, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %185 = getelementptr i8, ptr %69, i64 %153
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %185, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %186 = or disjoint i64 %120, 256
  %187 = getelementptr i8, ptr %27, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %187, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %188 = getelementptr i8, ptr %39, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %188, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %189 = getelementptr i8, ptr %40, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %189, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %190 = getelementptr i8, ptr %41, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %190, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %191 = getelementptr i8, ptr %42, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %191, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %192 = getelementptr i8, ptr %43, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %192, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %193 = getelementptr i8, ptr %44, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %193, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %194 = getelementptr i8, ptr %45, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %194, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %195 = getelementptr i8, ptr %46, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %195, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %196 = getelementptr i8, ptr %47, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %196, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %197 = getelementptr i8, ptr %48, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %197, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %198 = getelementptr i8, ptr %49, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %198, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %199 = getelementptr i8, ptr %50, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %199, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %200 = getelementptr i8, ptr %51, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %200, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %201 = getelementptr i8, ptr %52, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %201, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %202 = getelementptr i8, ptr %53, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %202, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %203 = getelementptr i8, ptr %54, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %203, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %204 = getelementptr i8, ptr %55, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %204, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %205 = getelementptr i8, ptr %56, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %205, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %206 = getelementptr i8, ptr %57, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %206, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %207 = getelementptr i8, ptr %58, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %207, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %208 = getelementptr i8, ptr %59, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %208, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %209 = getelementptr i8, ptr %60, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %209, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %210 = getelementptr i8, ptr %61, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %210, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %211 = getelementptr i8, ptr %62, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %211, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %212 = getelementptr i8, ptr %63, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %212, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %213 = getelementptr i8, ptr %64, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %213, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %214 = getelementptr i8, ptr %65, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %214, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %215 = getelementptr i8, ptr %66, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %215, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %216 = getelementptr i8, ptr %67, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %216, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %217 = getelementptr i8, ptr %68, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %217, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %218 = getelementptr i8, ptr %69, i64 %186
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %218, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %219 = or disjoint i64 %120, 384
  %220 = getelementptr i8, ptr %27, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %220, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %221 = getelementptr i8, ptr %39, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %221, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %222 = getelementptr i8, ptr %40, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %222, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %223 = getelementptr i8, ptr %41, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %223, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %224 = getelementptr i8, ptr %42, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %224, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %225 = getelementptr i8, ptr %43, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %225, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %226 = getelementptr i8, ptr %44, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %226, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %227 = getelementptr i8, ptr %45, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %227, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %228 = getelementptr i8, ptr %46, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %228, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %229 = getelementptr i8, ptr %47, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %229, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %230 = getelementptr i8, ptr %48, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %230, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %231 = getelementptr i8, ptr %49, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %231, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %232 = getelementptr i8, ptr %50, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %232, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %233 = getelementptr i8, ptr %51, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %233, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %234 = getelementptr i8, ptr %52, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %234, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %235 = getelementptr i8, ptr %53, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %235, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %236 = getelementptr i8, ptr %54, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %236, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %237 = getelementptr i8, ptr %55, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %237, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %238 = getelementptr i8, ptr %56, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %238, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %239 = getelementptr i8, ptr %57, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %239, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %240 = getelementptr i8, ptr %58, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %240, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %241 = getelementptr i8, ptr %59, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %241, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %242 = getelementptr i8, ptr %60, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %242, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %243 = getelementptr i8, ptr %61, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %243, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %244 = getelementptr i8, ptr %62, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %244, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %245 = getelementptr i8, ptr %63, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %245, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %246 = getelementptr i8, ptr %64, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %246, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %247 = getelementptr i8, ptr %65, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %247, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %248 = getelementptr i8, ptr %66, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %248, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %249 = getelementptr i8, ptr %67, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %249, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %250 = getelementptr i8, ptr %68, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %250, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %251 = getelementptr i8, ptr %69, i64 %219
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 4 dereferenceable(128) %251, i8 0, i64 128, i1 false), !alias.scope !21, !noalias !24
  %252 = add nuw nsw i64 %119, 1
  %253 = icmp eq i64 %252, 4
  br i1 %253, label %260, label %118

254:                                              ; preds = %257
  %255 = add nuw nsw i64 %261, 1
  %256 = icmp eq i64 %255, 4
  br i1 %256, label %117, label %260

257:                                              ; preds = %265
  %258 = add nuw nsw i64 %269, 1
  %259 = icmp eq i64 %258, 4
  br i1 %259, label %254, label %268

260:                                              ; preds = %118, %254
  %261 = phi i64 [ %255, %254 ], [ 0, %118 ]
  %262 = shl i64 %261, 14
  %263 = getelementptr i8, ptr %27, i64 %262
  %264 = getelementptr i8, ptr %15, i64 %262
  br label %268

265:                                              ; preds = %608
  %266 = add nuw nsw i64 %273, 1
  %267 = icmp eq i64 %266, 4
  br i1 %267, label %257, label %272

268:                                              ; preds = %260, %257
  %269 = phi i64 [ 0, %260 ], [ %258, %257 ]
  %270 = shl nsw i64 %269, 5
  %271 = getelementptr float, ptr %263, i64 %270
  br label %272

272:                                              ; preds = %268, %265
  %273 = phi i64 [ 0, %268 ], [ %266, %265 ]
  %274 = shl nsw i64 %273, 5
  %275 = shl i64 %273, 7
  %276 = shl i64 %273, 14
  %277 = or disjoint i64 %274, 1
  %278 = shl i64 %277, 2
  %279 = shl i64 %277, 9
  %280 = or disjoint i64 %274, 2
  %281 = shl i64 %280, 2
  %282 = shl i64 %280, 9
  %283 = or disjoint i64 %274, 3
  %284 = shl i64 %283, 2
  %285 = shl i64 %283, 9
  %286 = or disjoint i64 %274, 4
  %287 = shl i64 %286, 2
  %288 = shl i64 %286, 9
  %289 = or disjoint i64 %274, 5
  %290 = shl i64 %289, 2
  %291 = shl i64 %289, 9
  %292 = or disjoint i64 %274, 6
  %293 = shl i64 %292, 2
  %294 = shl i64 %292, 9
  %295 = or disjoint i64 %274, 7
  %296 = shl i64 %295, 2
  %297 = shl i64 %295, 9
  %298 = or disjoint i64 %274, 8
  %299 = shl i64 %298, 2
  %300 = shl i64 %298, 9
  %301 = or disjoint i64 %274, 9
  %302 = shl i64 %301, 2
  %303 = shl i64 %301, 9
  %304 = or disjoint i64 %274, 10
  %305 = shl i64 %304, 2
  %306 = shl i64 %304, 9
  %307 = or disjoint i64 %274, 11
  %308 = shl i64 %307, 2
  %309 = shl i64 %307, 9
  %310 = or disjoint i64 %274, 12
  %311 = shl i64 %310, 2
  %312 = shl i64 %310, 9
  %313 = or disjoint i64 %274, 13
  %314 = shl i64 %313, 2
  %315 = shl i64 %313, 9
  %316 = or disjoint i64 %274, 14
  %317 = shl i64 %316, 2
  %318 = shl i64 %316, 9
  %319 = or disjoint i64 %274, 15
  %320 = shl i64 %319, 2
  %321 = shl i64 %319, 9
  %322 = or disjoint i64 %274, 16
  %323 = shl i64 %322, 2
  %324 = shl i64 %322, 9
  %325 = or disjoint i64 %274, 17
  %326 = shl i64 %325, 2
  %327 = shl i64 %325, 9
  %328 = or disjoint i64 %274, 18
  %329 = shl i64 %328, 2
  %330 = shl i64 %328, 9
  %331 = or disjoint i64 %274, 19
  %332 = shl i64 %331, 2
  %333 = shl i64 %331, 9
  %334 = or disjoint i64 %274, 20
  %335 = shl i64 %334, 2
  %336 = shl i64 %334, 9
  %337 = or disjoint i64 %274, 21
  %338 = shl i64 %337, 2
  %339 = shl i64 %337, 9
  %340 = or disjoint i64 %274, 22
  %341 = shl i64 %340, 2
  %342 = shl i64 %340, 9
  %343 = or disjoint i64 %274, 23
  %344 = shl i64 %343, 2
  %345 = shl i64 %343, 9
  %346 = or disjoint i64 %274, 24
  %347 = shl i64 %346, 2
  %348 = shl i64 %346, 9
  %349 = or disjoint i64 %274, 25
  %350 = shl i64 %349, 2
  %351 = shl i64 %349, 9
  %352 = or disjoint i64 %274, 26
  %353 = shl i64 %352, 2
  %354 = shl i64 %352, 9
  %355 = or disjoint i64 %274, 27
  %356 = shl i64 %355, 2
  %357 = shl i64 %355, 9
  %358 = or disjoint i64 %274, 28
  %359 = shl i64 %358, 2
  %360 = shl i64 %358, 9
  %361 = or disjoint i64 %274, 29
  %362 = shl i64 %361, 2
  %363 = shl i64 %361, 9
  %364 = or disjoint i64 %274, 30
  %365 = shl i64 %364, 2
  %366 = shl i64 %364, 9
  %367 = or disjoint i64 %274, 31
  %368 = shl i64 %367, 2
  %369 = shl i64 %367, 9
  br label %370

370:                                              ; preds = %272, %608
  %371 = phi i64 [ 0, %272 ], [ %609, %608 ]
  %372 = shl i64 %371, 9
  %373 = getelementptr i8, ptr %271, i64 %372
  %374 = getelementptr i8, ptr %264, i64 %372
  %375 = getelementptr i8, ptr %374, i64 %368
  %376 = load float, ptr %375, align 4, !alias.scope !27, !noalias !28
  %377 = getelementptr i8, ptr %374, i64 %365
  %378 = load float, ptr %377, align 4, !alias.scope !27, !noalias !28
  %379 = getelementptr i8, ptr %374, i64 %362
  %380 = load float, ptr %379, align 4, !alias.scope !27, !noalias !28
  %381 = getelementptr i8, ptr %374, i64 %359
  %382 = load float, ptr %381, align 4, !alias.scope !27, !noalias !28
  %383 = getelementptr i8, ptr %374, i64 %356
  %384 = load float, ptr %383, align 4, !alias.scope !27, !noalias !28
  %385 = getelementptr i8, ptr %374, i64 %353
  %386 = load float, ptr %385, align 4, !alias.scope !27, !noalias !28
  %387 = getelementptr i8, ptr %374, i64 %350
  %388 = load float, ptr %387, align 4, !alias.scope !27, !noalias !28
  %389 = getelementptr i8, ptr %374, i64 %347
  %390 = load float, ptr %389, align 4, !alias.scope !27, !noalias !28
  %391 = getelementptr i8, ptr %374, i64 %344
  %392 = load float, ptr %391, align 4, !alias.scope !27, !noalias !28
  %393 = getelementptr i8, ptr %374, i64 %341
  %394 = load float, ptr %393, align 4, !alias.scope !27, !noalias !28
  %395 = getelementptr i8, ptr %374, i64 %338
  %396 = load float, ptr %395, align 4, !alias.scope !27, !noalias !28
  %397 = getelementptr i8, ptr %374, i64 %335
  %398 = load float, ptr %397, align 4, !alias.scope !27, !noalias !28
  %399 = getelementptr i8, ptr %374, i64 %332
  %400 = load float, ptr %399, align 4, !alias.scope !27, !noalias !28
  %401 = getelementptr i8, ptr %374, i64 %329
  %402 = load float, ptr %401, align 4, !alias.scope !27, !noalias !28
  %403 = getelementptr i8, ptr %374, i64 %326
  %404 = load float, ptr %403, align 4, !alias.scope !27, !noalias !28
  %405 = getelementptr i8, ptr %374, i64 %323
  %406 = load float, ptr %405, align 4, !alias.scope !27, !noalias !28
  %407 = getelementptr i8, ptr %374, i64 %320
  %408 = load float, ptr %407, align 4, !alias.scope !27, !noalias !28
  %409 = getelementptr i8, ptr %374, i64 %317
  %410 = load float, ptr %409, align 4, !alias.scope !27, !noalias !28
  %411 = getelementptr i8, ptr %374, i64 %314
  %412 = load float, ptr %411, align 4, !alias.scope !27, !noalias !28
  %413 = getelementptr i8, ptr %374, i64 %311
  %414 = load float, ptr %413, align 4, !alias.scope !27, !noalias !28
  %415 = getelementptr i8, ptr %374, i64 %308
  %416 = load float, ptr %415, align 4, !alias.scope !27, !noalias !28
  %417 = getelementptr i8, ptr %374, i64 %305
  %418 = load float, ptr %417, align 4, !alias.scope !27, !noalias !28
  %419 = getelementptr i8, ptr %374, i64 %302
  %420 = load float, ptr %419, align 4, !alias.scope !27, !noalias !28
  %421 = getelementptr i8, ptr %374, i64 %299
  %422 = load float, ptr %421, align 4, !alias.scope !27, !noalias !28
  %423 = getelementptr i8, ptr %374, i64 %296
  %424 = load float, ptr %423, align 4, !alias.scope !27, !noalias !28
  %425 = getelementptr i8, ptr %374, i64 %293
  %426 = load float, ptr %425, align 4, !alias.scope !27, !noalias !28
  %427 = getelementptr i8, ptr %374, i64 %290
  %428 = load float, ptr %427, align 4, !alias.scope !27, !noalias !28
  %429 = getelementptr i8, ptr %374, i64 %287
  %430 = load float, ptr %429, align 4, !alias.scope !27, !noalias !28
  %431 = getelementptr i8, ptr %374, i64 %284
  %432 = load float, ptr %431, align 4, !alias.scope !27, !noalias !28
  %433 = getelementptr i8, ptr %374, i64 %281
  %434 = load float, ptr %433, align 4, !alias.scope !27, !noalias !28
  %435 = getelementptr i8, ptr %374, i64 %278
  %436 = load float, ptr %435, align 4, !alias.scope !27, !noalias !28
  %437 = getelementptr i8, ptr %374, i64 %275
  %438 = load float, ptr %437, align 4, !alias.scope !27, !noalias !28
  %439 = insertelement <4 x float> poison, float %438, i64 0
  %440 = shufflevector <4 x float> %439, <4 x float> poison, <4 x i32> zeroinitializer
  %441 = insertelement <4 x float> poison, float %436, i64 0
  %442 = shufflevector <4 x float> %441, <4 x float> poison, <4 x i32> zeroinitializer
  %443 = insertelement <4 x float> poison, float %434, i64 0
  %444 = shufflevector <4 x float> %443, <4 x float> poison, <4 x i32> zeroinitializer
  %445 = insertelement <4 x float> poison, float %432, i64 0
  %446 = shufflevector <4 x float> %445, <4 x float> poison, <4 x i32> zeroinitializer
  %447 = insertelement <4 x float> poison, float %430, i64 0
  %448 = shufflevector <4 x float> %447, <4 x float> poison, <4 x i32> zeroinitializer
  %449 = insertelement <4 x float> poison, float %428, i64 0
  %450 = shufflevector <4 x float> %449, <4 x float> poison, <4 x i32> zeroinitializer
  %451 = insertelement <4 x float> poison, float %426, i64 0
  %452 = shufflevector <4 x float> %451, <4 x float> poison, <4 x i32> zeroinitializer
  %453 = insertelement <4 x float> poison, float %424, i64 0
  %454 = shufflevector <4 x float> %453, <4 x float> poison, <4 x i32> zeroinitializer
  %455 = insertelement <4 x float> poison, float %422, i64 0
  %456 = shufflevector <4 x float> %455, <4 x float> poison, <4 x i32> zeroinitializer
  %457 = insertelement <4 x float> poison, float %420, i64 0
  %458 = shufflevector <4 x float> %457, <4 x float> poison, <4 x i32> zeroinitializer
  %459 = insertelement <4 x float> poison, float %418, i64 0
  %460 = shufflevector <4 x float> %459, <4 x float> poison, <4 x i32> zeroinitializer
  %461 = insertelement <4 x float> poison, float %416, i64 0
  %462 = shufflevector <4 x float> %461, <4 x float> poison, <4 x i32> zeroinitializer
  %463 = insertelement <4 x float> poison, float %414, i64 0
  %464 = shufflevector <4 x float> %463, <4 x float> poison, <4 x i32> zeroinitializer
  %465 = insertelement <4 x float> poison, float %412, i64 0
  %466 = shufflevector <4 x float> %465, <4 x float> poison, <4 x i32> zeroinitializer
  %467 = insertelement <4 x float> poison, float %410, i64 0
  %468 = shufflevector <4 x float> %467, <4 x float> poison, <4 x i32> zeroinitializer
  %469 = insertelement <4 x float> poison, float %408, i64 0
  %470 = shufflevector <4 x float> %469, <4 x float> poison, <4 x i32> zeroinitializer
  %471 = insertelement <4 x float> poison, float %406, i64 0
  %472 = shufflevector <4 x float> %471, <4 x float> poison, <4 x i32> zeroinitializer
  %473 = insertelement <4 x float> poison, float %404, i64 0
  %474 = shufflevector <4 x float> %473, <4 x float> poison, <4 x i32> zeroinitializer
  %475 = insertelement <4 x float> poison, float %402, i64 0
  %476 = shufflevector <4 x float> %475, <4 x float> poison, <4 x i32> zeroinitializer
  %477 = insertelement <4 x float> poison, float %400, i64 0
  %478 = shufflevector <4 x float> %477, <4 x float> poison, <4 x i32> zeroinitializer
  %479 = insertelement <4 x float> poison, float %398, i64 0
  %480 = shufflevector <4 x float> %479, <4 x float> poison, <4 x i32> zeroinitializer
  %481 = insertelement <4 x float> poison, float %396, i64 0
  %482 = shufflevector <4 x float> %481, <4 x float> poison, <4 x i32> zeroinitializer
  %483 = insertelement <4 x float> poison, float %394, i64 0
  %484 = shufflevector <4 x float> %483, <4 x float> poison, <4 x i32> zeroinitializer
  %485 = insertelement <4 x float> poison, float %392, i64 0
  %486 = shufflevector <4 x float> %485, <4 x float> poison, <4 x i32> zeroinitializer
  %487 = insertelement <4 x float> poison, float %390, i64 0
  %488 = shufflevector <4 x float> %487, <4 x float> poison, <4 x i32> zeroinitializer
  %489 = insertelement <4 x float> poison, float %388, i64 0
  %490 = shufflevector <4 x float> %489, <4 x float> poison, <4 x i32> zeroinitializer
  %491 = insertelement <4 x float> poison, float %386, i64 0
  %492 = shufflevector <4 x float> %491, <4 x float> poison, <4 x i32> zeroinitializer
  %493 = insertelement <4 x float> poison, float %384, i64 0
  %494 = shufflevector <4 x float> %493, <4 x float> poison, <4 x i32> zeroinitializer
  %495 = insertelement <4 x float> poison, float %382, i64 0
  %496 = shufflevector <4 x float> %495, <4 x float> poison, <4 x i32> zeroinitializer
  %497 = insertelement <4 x float> poison, float %380, i64 0
  %498 = shufflevector <4 x float> %497, <4 x float> poison, <4 x i32> zeroinitializer
  %499 = insertelement <4 x float> poison, float %378, i64 0
  %500 = shufflevector <4 x float> %499, <4 x float> poison, <4 x i32> zeroinitializer
  %501 = insertelement <4 x float> poison, float %376, i64 0
  %502 = shufflevector <4 x float> %501, <4 x float> poison, <4 x i32> zeroinitializer
  br label %503

503:                                              ; preds = %503, %370
  %504 = phi i64 [ 0, %370 ], [ %606, %503 ]
  %505 = add nuw nsw i64 %504, %270
  %506 = getelementptr float, ptr %373, i64 %504
  %507 = shl i64 %505, 2
  %508 = getelementptr i8, ptr %21, i64 %507
  %509 = load <4 x float>, ptr %506, align 4, !alias.scope !21, !noalias !24
  %510 = getelementptr i8, ptr %508, i64 %276
  %511 = load <4 x float>, ptr %510, align 4, !alias.scope !29, !noalias !30
  %512 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %440, <4 x float> %511, <4 x float> %509)
  %513 = getelementptr i8, ptr %508, i64 %279
  %514 = load <4 x float>, ptr %513, align 4, !alias.scope !29, !noalias !30
  %515 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %442, <4 x float> %514, <4 x float> %512)
  %516 = getelementptr i8, ptr %508, i64 %282
  %517 = load <4 x float>, ptr %516, align 4, !alias.scope !29, !noalias !30
  %518 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %444, <4 x float> %517, <4 x float> %515)
  %519 = getelementptr i8, ptr %508, i64 %285
  %520 = load <4 x float>, ptr %519, align 4, !alias.scope !29, !noalias !30
  %521 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %446, <4 x float> %520, <4 x float> %518)
  %522 = getelementptr i8, ptr %508, i64 %288
  %523 = load <4 x float>, ptr %522, align 4, !alias.scope !29, !noalias !30
  %524 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %448, <4 x float> %523, <4 x float> %521)
  %525 = getelementptr i8, ptr %508, i64 %291
  %526 = load <4 x float>, ptr %525, align 4, !alias.scope !29, !noalias !30
  %527 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %450, <4 x float> %526, <4 x float> %524)
  %528 = getelementptr i8, ptr %508, i64 %294
  %529 = load <4 x float>, ptr %528, align 4, !alias.scope !29, !noalias !30
  %530 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %452, <4 x float> %529, <4 x float> %527)
  %531 = getelementptr i8, ptr %508, i64 %297
  %532 = load <4 x float>, ptr %531, align 4, !alias.scope !29, !noalias !30
  %533 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %454, <4 x float> %532, <4 x float> %530)
  %534 = getelementptr i8, ptr %508, i64 %300
  %535 = load <4 x float>, ptr %534, align 4, !alias.scope !29, !noalias !30
  %536 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %456, <4 x float> %535, <4 x float> %533)
  %537 = getelementptr i8, ptr %508, i64 %303
  %538 = load <4 x float>, ptr %537, align 4, !alias.scope !29, !noalias !30
  %539 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %458, <4 x float> %538, <4 x float> %536)
  %540 = getelementptr i8, ptr %508, i64 %306
  %541 = load <4 x float>, ptr %540, align 4, !alias.scope !29, !noalias !30
  %542 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %460, <4 x float> %541, <4 x float> %539)
  %543 = getelementptr i8, ptr %508, i64 %309
  %544 = load <4 x float>, ptr %543, align 4, !alias.scope !29, !noalias !30
  %545 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %462, <4 x float> %544, <4 x float> %542)
  %546 = getelementptr i8, ptr %508, i64 %312
  %547 = load <4 x float>, ptr %546, align 4, !alias.scope !29, !noalias !30
  %548 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %464, <4 x float> %547, <4 x float> %545)
  %549 = getelementptr i8, ptr %508, i64 %315
  %550 = load <4 x float>, ptr %549, align 4, !alias.scope !29, !noalias !30
  %551 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %466, <4 x float> %550, <4 x float> %548)
  %552 = getelementptr i8, ptr %508, i64 %318
  %553 = load <4 x float>, ptr %552, align 4, !alias.scope !29, !noalias !30
  %554 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %468, <4 x float> %553, <4 x float> %551)
  %555 = getelementptr i8, ptr %508, i64 %321
  %556 = load <4 x float>, ptr %555, align 4, !alias.scope !29, !noalias !30
  %557 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %470, <4 x float> %556, <4 x float> %554)
  %558 = getelementptr i8, ptr %508, i64 %324
  %559 = load <4 x float>, ptr %558, align 4, !alias.scope !29, !noalias !30
  %560 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %472, <4 x float> %559, <4 x float> %557)
  %561 = getelementptr i8, ptr %508, i64 %327
  %562 = load <4 x float>, ptr %561, align 4, !alias.scope !29, !noalias !30
  %563 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %474, <4 x float> %562, <4 x float> %560)
  %564 = getelementptr i8, ptr %508, i64 %330
  %565 = load <4 x float>, ptr %564, align 4, !alias.scope !29, !noalias !30
  %566 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %476, <4 x float> %565, <4 x float> %563)
  %567 = getelementptr i8, ptr %508, i64 %333
  %568 = load <4 x float>, ptr %567, align 4, !alias.scope !29, !noalias !30
  %569 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %478, <4 x float> %568, <4 x float> %566)
  %570 = getelementptr i8, ptr %508, i64 %336
  %571 = load <4 x float>, ptr %570, align 4, !alias.scope !29, !noalias !30
  %572 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %480, <4 x float> %571, <4 x float> %569)
  %573 = getelementptr i8, ptr %508, i64 %339
  %574 = load <4 x float>, ptr %573, align 4, !alias.scope !29, !noalias !30
  %575 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %482, <4 x float> %574, <4 x float> %572)
  %576 = getelementptr i8, ptr %508, i64 %342
  %577 = load <4 x float>, ptr %576, align 4, !alias.scope !29, !noalias !30
  %578 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %484, <4 x float> %577, <4 x float> %575)
  %579 = getelementptr i8, ptr %508, i64 %345
  %580 = load <4 x float>, ptr %579, align 4, !alias.scope !29, !noalias !30
  %581 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %486, <4 x float> %580, <4 x float> %578)
  %582 = getelementptr i8, ptr %508, i64 %348
  %583 = load <4 x float>, ptr %582, align 4, !alias.scope !29, !noalias !30
  %584 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %488, <4 x float> %583, <4 x float> %581)
  %585 = getelementptr i8, ptr %508, i64 %351
  %586 = load <4 x float>, ptr %585, align 4, !alias.scope !29, !noalias !30
  %587 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %490, <4 x float> %586, <4 x float> %584)
  %588 = getelementptr i8, ptr %508, i64 %354
  %589 = load <4 x float>, ptr %588, align 4, !alias.scope !29, !noalias !30
  %590 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %492, <4 x float> %589, <4 x float> %587)
  %591 = getelementptr i8, ptr %508, i64 %357
  %592 = load <4 x float>, ptr %591, align 4, !alias.scope !29, !noalias !30
  %593 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %494, <4 x float> %592, <4 x float> %590)
  %594 = getelementptr i8, ptr %508, i64 %360
  %595 = load <4 x float>, ptr %594, align 4, !alias.scope !29, !noalias !30
  %596 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %496, <4 x float> %595, <4 x float> %593)
  %597 = getelementptr i8, ptr %508, i64 %363
  %598 = load <4 x float>, ptr %597, align 4, !alias.scope !29, !noalias !30
  %599 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %498, <4 x float> %598, <4 x float> %596)
  %600 = getelementptr i8, ptr %508, i64 %366
  %601 = load <4 x float>, ptr %600, align 4, !alias.scope !29, !noalias !30
  %602 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %500, <4 x float> %601, <4 x float> %599)
  %603 = getelementptr i8, ptr %508, i64 %369
  %604 = load <4 x float>, ptr %603, align 4, !alias.scope !29, !noalias !30
  %605 = call <4 x float> @llvm.fmuladd.v4f32(<4 x float> %502, <4 x float> %604, <4 x float> %602)
  store <4 x float> %605, ptr %506, align 4, !alias.scope !21, !noalias !24
  %606 = add nuw i64 %504, 4
  %607 = icmp eq i64 %606, 32
  br i1 %607, label %608, label %503, !llvm.loop !31

608:                                              ; preds = %503
  %609 = add nuw nsw i64 %371, 1
  %610 = icmp eq i64 %609, 32
  br i1 %610, label %265, label %370

611:                                              ; preds = %623, %2
  %612 = phi i64 [ 42, %2 ], [ %629, %623 ]
  %613 = phi i64 [ 0, %2 ], [ %627, %623 ]
  %614 = lshr i64 %612, 30
  %615 = xor i64 %614, %612
  %616 = mul nuw nsw i64 %615, 1812433253
  %617 = or disjoint i64 %613, 1
  %618 = add nuw i64 %616, %617
  %619 = and i64 %618, 4294967295
  %620 = shl nuw nsw i64 %613, 3
  %621 = getelementptr i8, ptr %7, i64 %620
  store i64 %619, ptr %621, align 8, !alias.scope !34, !noalias !37
  %622 = icmp eq i64 %613, 622
  br i1 %622, label %8, label %623, !llvm.loop !38

623:                                              ; preds = %611
  %624 = lshr i64 %619, 30
  %625 = xor i64 %624, %618
  %626 = mul i64 %625, 1812433253
  %627 = add nuw nsw i64 %613, 2
  %628 = add i64 %626, %627
  %629 = and i64 %628, 4294967295
  %630 = shl nuw nsw i64 %617, 3
  %631 = getelementptr i8, ptr %7, i64 %630
  store i64 %629, ptr %631, align 8, !alias.scope !34, !noalias !37
  br label %611
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: mustprogress norecurse nounwind uwtable
define internal fastcc void @_ZL11fill_randomPfiRSt23mersenne_twister_engineImLm32ELm624ELm397ELm31ELm2567483615ELm11ELm4294967295ELm7ELm2636928640ELm15ELm4022730752ELm18ELm1812433253EE(ptr nocapture noundef writeonly %0, ptr nocapture noundef nonnull align 8 dereferenceable(5000) %1) unnamed_addr #2 {
  %3 = tail call x86_fp80 @llvm.log.f80(x86_fp80 0xK401F8000000000000000), !tbaa !39
  %4 = tail call x86_fp80 @llvm.log.f80(x86_fp80 0xK40008000000000000000), !tbaa !39
  %5 = fdiv x86_fp80 %3, %4
  %6 = fptoui x86_fp80 %5 to i64
  %7 = add i64 %6, 23
  %8 = udiv i64 %7, %6
  %9 = tail call i64 @llvm.umax.i64(i64 %8, i64 1)
  %10 = getelementptr inbounds nuw i8, ptr %1, i64 4992
  %11 = getelementptr inbounds nuw i8, ptr %1, i64 1816
  %12 = getelementptr inbounds nuw i8, ptr %1, i64 4984
  %13 = getelementptr inbounds nuw i8, ptr %1, i64 3168
  %14 = load i64, ptr %10, align 8, !tbaa !9
  %15 = getelementptr inbounds nuw i8, ptr %1, i64 8
  %16 = getelementptr inbounds nuw i8, ptr %1, i64 3176
  %17 = getelementptr inbounds nuw i8, ptr %1, i64 1824
  %18 = getelementptr inbounds nuw i8, ptr %1, i64 1816
  %19 = getelementptr inbounds nuw i8, ptr %1, i64 4984
  %20 = getelementptr inbounds nuw i8, ptr %1, i64 1808
  br label %22

21:                                               ; preds = %70
  ret void

22:                                               ; preds = %2, %70
  %23 = phi i64 [ %14, %2 ], [ %49, %70 ]
  %24 = phi i64 [ 0, %2 ], [ %74, %70 ]
  br label %28

25:                                               ; preds = %47
  %26 = fdiv float %64, %65
  %27 = fcmp ult float %26, 1.000000e+00
  br i1 %27, label %70, label %68, !prof !41

28:                                               ; preds = %47, %22
  %29 = phi i64 [ %23, %22 ], [ %49, %47 ]
  %30 = phi i64 [ %9, %22 ], [ %66, %47 ]
  %31 = phi float [ 1.000000e+00, %22 ], [ %65, %47 ]
  %32 = phi float [ 0.000000e+00, %22 ], [ %64, %47 ]
  %33 = icmp ugt i64 %29, 623
  br i1 %33, label %76, label %47

34:                                               ; preds = %113
  %35 = load i64, ptr %12, align 8, !alias.scope !42, !noalias !37
  %36 = and i64 %35, -2147483648
  %37 = load i64, ptr %1, align 8, !alias.scope !42, !noalias !37
  %38 = and i64 %37, 2147483646
  %39 = or disjoint i64 %38, %36
  %40 = load i64, ptr %13, align 8, !alias.scope !42, !noalias !37
  %41 = lshr exact i64 %39, 1
  %42 = xor i64 %41, %40
  %43 = and i64 %37, 1
  %44 = icmp eq i64 %43, 0
  %45 = select i1 %44, i64 0, i64 2567483615
  %46 = xor i64 %42, %45
  store i64 %46, ptr %12, align 8, !alias.scope !42, !noalias !37
  br label %47

47:                                               ; preds = %34, %28
  %48 = phi i64 [ %29, %28 ], [ 0, %34 ]
  %49 = add nuw nsw i64 %48, 1
  store i64 %49, ptr %10, align 8, !tbaa !9
  %50 = getelementptr inbounds nuw [624 x i64], ptr %1, i64 0, i64 %48
  %51 = load i64, ptr %50, align 8, !tbaa !5
  %52 = lshr i64 %51, 11
  %53 = and i64 %52, 4294967295
  %54 = xor i64 %53, %51
  %55 = shl i64 %54, 7
  %56 = and i64 %55, 2636928640
  %57 = xor i64 %56, %54
  %58 = shl i64 %57, 15
  %59 = and i64 %58, 4022730752
  %60 = xor i64 %59, %57
  %61 = lshr i64 %60, 18
  %62 = xor i64 %61, %60
  %63 = uitofp i64 %62 to float
  %64 = tail call float @llvm.fmuladd.f32(float %63, float %31, float %32)
  %65 = fmul float %31, 0x41F0000000000000
  %66 = add i64 %30, -1
  %67 = icmp eq i64 %66, 0
  br i1 %67, label %25, label %28, !llvm.loop !45

68:                                               ; preds = %25
  %69 = tail call noundef float @nextafterf(float noundef 1.000000e+00, float noundef 0.000000e+00) #11, !tbaa !39
  br label %70

70:                                               ; preds = %25, %68
  %71 = phi float [ %69, %68 ], [ %26, %25 ]
  %72 = fadd float %71, 0.000000e+00
  %73 = getelementptr inbounds nuw float, ptr %0, i64 %24
  store float %72, ptr %73, align 4, !tbaa !17
  %74 = add nuw nsw i64 %24, 1
  %75 = icmp eq i64 %74, 16384
  br i1 %75, label %21, label %22, !llvm.loop !46

76:                                               ; preds = %28
  %77 = load i64, ptr %1, align 8, !tbaa !5
  %78 = insertelement <2 x i64> poison, i64 %77, i64 1
  br label %79

79:                                               ; preds = %79, %76
  %80 = phi i64 [ 0, %76 ], [ %98, %79 ]
  %81 = phi <2 x i64> [ %78, %76 ], [ %84, %79 ]
  %82 = shl nuw nsw i64 %80, 3
  %83 = getelementptr i8, ptr %15, i64 %82
  %84 = load <2 x i64>, ptr %83, align 8, !alias.scope !42, !noalias !37
  %85 = shufflevector <2 x i64> %81, <2 x i64> %84, <2 x i32> <i32 1, i32 2>
  %86 = and <2 x i64> %85, splat (i64 -2147483648)
  %87 = and <2 x i64> %84, splat (i64 2147483646)
  %88 = or disjoint <2 x i64> %87, %86
  %89 = getelementptr i8, ptr %16, i64 %82
  %90 = load <2 x i64>, ptr %89, align 8, !alias.scope !42, !noalias !37
  %91 = lshr exact <2 x i64> %88, splat (i64 1)
  %92 = xor <2 x i64> %91, %90
  %93 = and <2 x i64> %84, splat (i64 1)
  %94 = icmp eq <2 x i64> %93, zeroinitializer
  %95 = select <2 x i1> %94, <2 x i64> zeroinitializer, <2 x i64> splat (i64 2567483615)
  %96 = xor <2 x i64> %92, %95
  %97 = getelementptr i8, ptr %1, i64 %82
  store <2 x i64> %96, ptr %97, align 8, !alias.scope !42, !noalias !37
  %98 = add nuw i64 %80, 2
  %99 = icmp eq i64 %98, 226
  br i1 %99, label %100, label %79, !llvm.loop !47

100:                                              ; preds = %79
  %101 = extractelement <2 x i64> %84, i64 1
  %102 = and i64 %101, -2147483648
  %103 = load i64, ptr %18, align 8, !alias.scope !42, !noalias !37
  %104 = and i64 %103, 2147483646
  %105 = or disjoint i64 %104, %102
  %106 = load i64, ptr %19, align 8, !alias.scope !42, !noalias !37
  %107 = lshr exact i64 %105, 1
  %108 = xor i64 %107, %106
  %109 = and i64 %103, 1
  %110 = icmp eq i64 %109, 0
  %111 = select i1 %110, i64 0, i64 2567483615
  %112 = xor i64 %108, %111
  store i64 %112, ptr %20, align 8, !alias.scope !42, !noalias !37
  br label %113

113:                                              ; preds = %100, %133
  %114 = phi i64 [ %135, %133 ], [ 0, %100 ]
  %115 = getelementptr i64, ptr %11, i64 %114
  %116 = load i64, ptr %115, align 8, !alias.scope !42, !noalias !37
  %117 = and i64 %116, -2147483648
  %118 = shl nuw nsw i64 %114, 3
  %119 = getelementptr i8, ptr %17, i64 %118
  %120 = load i64, ptr %119, align 8, !alias.scope !42, !noalias !37
  %121 = and i64 %120, 2147483646
  %122 = or disjoint i64 %121, %117
  %123 = getelementptr i8, ptr %1, i64 %118
  %124 = load i64, ptr %123, align 8, !alias.scope !42, !noalias !37
  %125 = lshr exact i64 %122, 1
  %126 = xor i64 %125, %124
  %127 = and i64 %120, 1
  %128 = icmp eq i64 %127, 0
  %129 = select i1 %128, i64 0, i64 2567483615
  %130 = xor i64 %126, %129
  %131 = getelementptr i8, ptr %11, i64 %118
  store i64 %130, ptr %131, align 8, !alias.scope !42, !noalias !37
  %132 = icmp eq i64 %114, 395
  br i1 %132, label %34, label %133

133:                                              ; preds = %113
  %134 = getelementptr i64, ptr %17, i64 %114
  store i64 %120, ptr %134, align 8, !alias.scope !42, !noalias !37
  %135 = add nuw nsw i64 %114, 1
  br label %113
}

; Function Attrs: mustprogress nounwind willreturn allockind("free") memory(argmem: readwrite, inaccessiblemem: readwrite)
declare void @free(ptr allocptr nocapture noundef) local_unnamed_addr #3

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nofree nounwind
declare i32 @posix_memalign(ptr noundef, i64 noundef, i64 noundef) local_unnamed_addr #4

; Function Attrs: inlinehint mustprogress uwtable
declare noundef nonnull align 8 dereferenceable(8) ptr @_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc(ptr noundef nonnull align 8 dereferenceable(8), ptr noundef) local_unnamed_addr #5

; Function Attrs: nofree noreturn nounwind
declare void @exit(i32 noundef) local_unnamed_addr #6

; Function Attrs: mustprogress nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare float @llvm.fmuladd.f32(float, float, float) #7

; Function Attrs: nounwind
declare float @nextafterf(float noundef, float noundef) local_unnamed_addr #8

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare x86_fp80 @llvm.log.f80(x86_fp80) #9

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i64 @llvm.umax.i64(i64, i64) #9

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr nocapture writeonly, i8, i64, i1 immarg) #10

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare <4 x float> @llvm.fmuladd.v4f32(<4 x float>, <4 x float>, <4 x float>) #9

attributes #0 = { mustprogress norecurse uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "polly-optimized" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { mustprogress norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "polly-optimized" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { mustprogress nounwind willreturn allockind("free") memory(argmem: readwrite, inaccessiblemem: readwrite) "alloc-family"="malloc" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { inlinehint mustprogress uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #6 = { nofree noreturn nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #7 = { mustprogress nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #8 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #9 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #10 = { nocallback nofree nounwind willreturn memory(argmem: write) }
attributes #11 = { nounwind }
attributes #12 = { cold noreturn nounwind }

!llvm.module.flags = !{!0, !1, !2, !3}
!llvm.ident = !{!4}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{!"Ubuntu clang version 20.1.8 (0ubuntu4)"}
!5 = !{!6, !6, i64 0}
!6 = !{!"long", !7, i64 0}
!7 = !{!"omnipotent char", !8, i64 0}
!8 = !{!"Simple C++ TBAA"}
!9 = !{!10, !6, i64 4992}
!10 = !{!"_ZTSSt23mersenne_twister_engineImLm32ELm624ELm397ELm31ELm2567483615ELm11ELm4294967295ELm7ELm2636928640ELm15ELm4022730752ELm18ELm1812433253EE", !7, i64 0, !6, i64 4992}
!11 = !{!12, !12, i64 0}
!12 = !{!"p1 float", !13, i64 0}
!13 = !{!"any pointer", !7, i64 0}
!14 = distinct !{!14, !15, !16}
!15 = !{!"llvm.loop.mustprogress"}
!16 = !{!"llvm.loop.vectorize.enable", i32 0}
!17 = !{!18, !18, i64 0}
!18 = !{!"float", !7, i64 0}
!19 = distinct !{!19, !15, !16}
!20 = distinct !{!20, !15, !16}
!21 = !{!22}
!22 = distinct !{!22, !23, !"polly.alias.scope.MemRef4"}
!23 = distinct !{!23, !"polly.alias.scope.domain"}
!24 = !{!25, !26}
!25 = distinct !{!25, !23, !"polly.alias.scope.MemRef1"}
!26 = distinct !{!26, !23, !"polly.alias.scope.MemRef2"}
!27 = !{!25}
!28 = !{!26, !22}
!29 = !{!26}
!30 = !{!25, !22}
!31 = distinct !{!31, !32, !33}
!32 = !{!"llvm.loop.isvectorized", i32 1}
!33 = !{!"llvm.loop.unroll.runtime.disable"}
!34 = !{!35}
!35 = distinct !{!35, !36, !"polly.alias.scope.MemRef1"}
!36 = distinct !{!36, !"polly.alias.scope.domain"}
!37 = !{}
!38 = distinct !{!38, !15}
!39 = !{!40, !40, i64 0}
!40 = !{!"int", !7, i64 0}
!41 = !{!"branch_weights", !"expected", i32 2000, i32 1}
!42 = !{!43}
!43 = distinct !{!43, !44, !"polly.alias.scope.MemRef1"}
!44 = distinct !{!44, !"polly.alias.scope.domain"}
!45 = distinct !{!45, !15}
!46 = distinct !{!46, !15}
!47 = distinct !{!47, !15, !32, !33}
