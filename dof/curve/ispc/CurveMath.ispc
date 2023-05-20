#include "MacrosIspc.h"

// Reference: https://github.com/warrenm/AHEasing/blob/master/AHEasing/easing.c

EXPORT void zero(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        outT[i] = 0.0f;
    }
}

EXPORT void one(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        outT[i] = 1.0f;
    }
}

EXPORT void linearInterpolation(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        outT[i] = inT[i];
    }
}

EXPORT void quadraticEaseIn(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        outT[i] = inT[i]*inT[i];
    }
}

EXPORT void quadraticEaseOut(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        outT[i] = -(p * (p - 2));
    }
}

EXPORT void quadraticEaseInOut(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        if(p < 0.5) {
            outT[i] = 2 * p * p;
        }
        else {
            outT[i] = (-2 * p * p) + (4 * p) - 1;
        }
    }
}

EXPORT void cubicEaseIn(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        outT[i] = p * p * p;
    }
}

EXPORT void cubicEaseOut(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        const float f = (p - 1);
        outT[i] = f * f * f + 1;
    }
}

EXPORT void cubicEaseInOut(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        if(p < 0.5) {
            outT[i] = 4 * p * p * p;
        }
        else {
            const float f = ((2 * p) - 2);
            outT[i] = 0.5 * f * f * f + 1;
        }
    }
}

EXPORT void quarticEaseIn(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        outT[i] = p * p * p * p;
    }
}

EXPORT void quarticEaseOut(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        const float f = (p - 1);
        outT[i] = f * f * f * (1 - p) + 1;
    }
}

EXPORT void quarticEaseInOut(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        if(p < 0.5) {
            outT[i] = 8 * p * p * p * p;
        }
        else {
            const float f = (p - 1);
            outT[i] = -8 * f * f * f * f + 1;
        }
    }
}

EXPORT void quinticEaseIn(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        outT[i] = p * p * p * p * p;
    }
}

EXPORT void quinticEaseOut(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float f = (inT[i] - 1);
        outT[i] = f * f * f * f * f + 1;
    }
}

EXPORT void quinticEaseInOut(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        if(p < 0.5) {
            outT[i] = 16 * p * p * p * p * p;
        }
        else {
            const float f = ((2 * p) - 2);
            outT[i] = 0.5 * f * f * f * f * f + 1;
        }
    }
}

const float M_PI = PI;
const float M_PI_2 = PI * 0.5f;

EXPORT void sineEaseIn(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        outT[i] = sin((p - 1) * M_PI_2) + 1;
    }
}

EXPORT void sineEaseOut(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        outT[i] = sin(p * M_PI_2);
    }
}

EXPORT void sineEaseInOut(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        outT[i] = 0.5 * (1 - cos(p * M_PI));
    }
}

EXPORT void circularEaseIn(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        outT[i] = 1 - sqrt(1 - (p * p));
    }
}

EXPORT void circularEaseOut(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        outT[i] = sqrt((2 - p) * p);
    }
}

EXPORT void circularEaseInOut(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        if(p < 0.5) {
            outT[i] = 0.5 * (1 - sqrt(1 - 4 * (p * p)));
        }
        else {
            outT[i] = 0.5 * (sqrt(-((2 * p) - 3) * ((2 * p) - 1)) + 1);
        }
    }
}

EXPORT void exponentialEaseIn(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        outT[i] = (p == 0.0) ? p : pow(2, 10 * (p - 1));
    }
}

EXPORT void exponentialEaseOut(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        outT[i] = (p == 1.0) ? p : 1 - pow(2, -10 * p);
    }
}

EXPORT void exponentialEaseInOut(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        float result = p;
        if(p < 0.5) {
            result = 0.5 * pow(2, (20 * p) - 10);
        }
        else {
            result = -0.5 * pow(2, (-20 * p) + 10) + 1;
        }
        clamp(result, 0.0f, 1.0f);
        outT[i] = result;
    }
}

EXPORT void elasticEaseIn(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        outT[i] = sin(13 * M_PI_2 * p) * pow(2, 10 * (p - 1));
    }
}

EXPORT void elasticEaseOut(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        outT[i] = sin(-13 * M_PI_2 * (p + 1)) * pow(2, -10 * p) + 1;
    }
}

EXPORT void elasticEaseInOut(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        if(p < 0.5) {
            outT[i] = 0.5 * sin(13 * M_PI_2 * (2 * p)) * pow(2, 10 * ((2 * p) - 1));
        }
        else {
            outT[i] = 0.5 * (sin(-13 * M_PI_2 * ((2 * p - 1) + 1)) * pow(2, -10 * (2 * p - 1)) + 2);
        }
    }
}

EXPORT void backEaseIn(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        outT[i] = p * p * p - p * sin(p * M_PI);
    }
}

EXPORT void backEaseOut(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float f = (1 - inT[i]);
        outT[i] = 1 - (f * f * f - f * sin(f * M_PI));
    }
}

EXPORT void backEaseInOut(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        const float p = inT[i];
        if(p < 0.5) {
            const float f = 2 * p;
            outT[i] = 0.5 * (f * f * f - f * sin(f * M_PI));
        }
        else {
            const float f = (1 - (2*p - 1));
            outT[i] = 0.5 * (1 - (f * f * f - f * sin(f * M_PI))) + 0.5;
        }
    }
}

float bounceEaseOutVarying(float p) {
    if(p < 4/11.0)
    {
        return (121 * p * p)/16.0;
    }
    else if(p < 8/11.0)
    {
        return (363/40.0 * p * p) - (99/10.0 * p) + 17/5.0;
    }
    else if(p < 9/10.0)
    {
        return (4356/361.0 * p * p) - (35442/1805.0 * p) + 16061/1805.0;
    }
    else
    {
        return (54/5.0 * p * p) - (513/25.0 * p) + 268/25.0;
    }
}

float bounceEaseInVarying(float p) {
    return 1 - bounceEaseOutVarying(1 - p);
}

float bounceEaseInOutVarying(float p) {
    if(p < 0.5) {
        return 0.5 * bounceEaseInVarying(p*2);
    }
    else {
        return 0.5 * bounceEaseOutVarying(p * 2 - 1) + 0.5;
    }
}

EXPORT void bounceEaseIn(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        outT[i] = bounceEaseInVarying(inT[i]);
    }
}

EXPORT void bounceEaseOut(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        outT[i] = bounceEaseOutVarying(inT[i]);
    }
}

EXPORT void bounceEaseInOut(const UNIFORM float inT[], UNIFORM float outT[], UNIFORM int count) {
    FOREACH(i, 0, count) {
        outT[i] = bounceEaseInOutVarying(inT[i]);
    }
}