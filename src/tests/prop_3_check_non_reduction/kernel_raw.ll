; ModuleID = 'kernel.cpp'
source_filename = "kernel.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: mustprogress noinline nounwind uwtable
define dso_local void @reduction_1(ptr noalias noundef %A, ptr noalias noundef %T, i32 noundef %N) #0 {
entry:
  %A.addr = alloca ptr, align 8
  %T.addr = alloca ptr, align 8
  %N.addr = alloca i32, align 4
  %S = alloca [1 x double], align 8
  %i = alloca i32, align 4
  store ptr %A, ptr %A.addr, align 8
  store ptr %T, ptr %T.addr, align 8
  store i32 %N, ptr %N.addr, align 4
  call void @llvm.memset.p0.i64(ptr align 8 %S, i8 0, i64 8, i1 false)
  store i32 128, ptr %N.addr, align 4
  store i32 0, ptr %i, align 4
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %0 = load i32, ptr %i, align 4
  %1 = load i32, ptr %N.addr, align 4
  %cmp = icmp slt i32 %0, %1
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %arrayidx = getelementptr inbounds [1 x double], ptr %S, i64 0, i64 0
  %2 = load double, ptr %arrayidx, align 8
  %add = fadd fast double %2, 5.000000e+00
  %conv = fptrunc fast double %add to float
  %3 = load ptr, ptr %T.addr, align 8
  %arrayidx1 = getelementptr inbounds float, ptr %3, i64 0
  store float %conv, ptr %arrayidx1, align 4
  %4 = load ptr, ptr %A.addr, align 8
  %5 = load i32, ptr %i, align 4
  %idxprom = sext i32 %5 to i64
  %arrayidx2 = getelementptr inbounds float, ptr %4, i64 %idxprom
  %6 = load float, ptr %arrayidx2, align 4
  %add3 = fadd fast float %6, 2.000000e+00
  %conv4 = fpext fast float %add3 to double
  %arrayidx5 = getelementptr inbounds [1 x double], ptr %S, i64 0, i64 0
  store double %conv4, ptr %arrayidx5, align 8
  br label %for.inc

for.inc:                                          ; preds = %for.body
  %7 = load i32, ptr %i, align 4
  %inc = add nsw i32 %7, 1
  store i32 %inc, ptr %i, align 4
  br label %for.cond, !llvm.loop !6

for.end:                                          ; preds = %for.cond
  ret void
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr writeonly captures(none), i8, i64, i1 immarg) #1

attributes #0 = { mustprogress noinline nounwind uwtable "approx-func-fp-math"="true" "frame-pointer"="all" "min-legal-vector-width"="0" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: write) }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"clang version 21.1.8 (https://github.com/llvm/llvm-project.git 2078da43e25a4623cab2d0d60decddf709aaea28)"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}
