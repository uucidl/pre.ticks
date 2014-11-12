class WithBlendEnabledScope
{
public:
        WithBlendEnabledScope(GLuint src, GLuint dst)
        {
                glEnable(GL_BLEND);
                glBlendFunc(src, dst);
        }

        ~WithBlendEnabledScope()
        {
                glDisable(GL_BLEND);
        }

private:
        WithBlendEnabledScope(WithBlendEnabledScope&) = delete;
        WithBlendEnabledScope(WithBlendEnabledScope&&) = delete;
        WithBlendEnabledScope& operator=(WithBlendEnabledScope&) = delete;
        WithBlendEnabledScope& operator=(WithBlendEnabledScope&&) = delete;
};
