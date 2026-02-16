"""Authentication API — register, login, profile."""

from fastapi import APIRouter, Depends, HTTPException, status
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
from pydantic import BaseModel, EmailStr
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.db.database import get_db
from app.models.user import User
from app.services.auth import (
    hash_password,
    verify_password,
    create_access_token,
    decode_access_token,
)

router = APIRouter(prefix="/auth", tags=["Autentisering"])
security = HTTPBearer(auto_error=False)


# --- Schemas ---

class RegisterRequest(BaseModel):
    email: str
    name: str
    password: str


class LoginRequest(BaseModel):
    email: str
    password: str


class AuthResponse(BaseModel):
    access_token: str
    token_type: str = "bearer"
    user: "UserProfile"


class UserProfile(BaseModel):
    id: int
    email: str
    name: str
    created_at: str

    model_config = {"from_attributes": True}


# --- Dependency: get current user ---

async def get_current_user(
    credentials: HTTPAuthorizationCredentials | None = Depends(security),
    db: AsyncSession = Depends(get_db),
) -> User | None:
    """Extract the current user from JWT token. Returns None if not authenticated."""
    if credentials is None:
        return None

    payload = decode_access_token(credentials.credentials)
    if payload is None:
        return None

    user_id = payload.get("sub")
    if user_id is None:
        return None

    result = await db.execute(select(User).where(User.id == int(user_id)))
    return result.scalar_one_or_none()


async def require_user(
    user: User | None = Depends(get_current_user),
) -> User:
    """Dependency that requires authentication. Raises 401 if not logged in."""
    if user is None:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Ikke logget inn. Vennligst logg inn først.",
            headers={"WWW-Authenticate": "Bearer"},
        )
    return user


# --- Endpoints ---

@router.post("/register", response_model=AuthResponse)
async def register(data: RegisterRequest, db: AsyncSession = Depends(get_db)):
    """Registrer en ny bruker."""
    # Check if email already exists
    existing = await db.execute(select(User).where(User.email == data.email.lower()))
    if existing.scalar_one_or_none():
        raise HTTPException(status_code=409, detail="E-post er allerede registrert")

    if len(data.password) < 6:
        raise HTTPException(status_code=400, detail="Passord må være minst 6 tegn")

    user = User(
        email=data.email.lower().strip(),
        name=data.name.strip(),
        password_hash=hash_password(data.password),
    )
    db.add(user)
    await db.flush()

    token = create_access_token({"sub": str(user.id)})

    return AuthResponse(
        access_token=token,
        user=UserProfile(
            id=user.id,
            email=user.email,
            name=user.name,
            created_at=str(user.created_at),
        ),
    )


@router.post("/login", response_model=AuthResponse)
async def login(data: LoginRequest, db: AsyncSession = Depends(get_db)):
    """Logg inn med e-post og passord."""
    result = await db.execute(select(User).where(User.email == data.email.lower()))
    user = result.scalar_one_or_none()

    if not user or not verify_password(data.password, user.password_hash):
        raise HTTPException(status_code=401, detail="Feil e-post eller passord")

    token = create_access_token({"sub": str(user.id)})

    return AuthResponse(
        access_token=token,
        user=UserProfile(
            id=user.id,
            email=user.email,
            name=user.name,
            created_at=str(user.created_at),
        ),
    )


@router.get("/me", response_model=UserProfile)
async def get_profile(user: User = Depends(require_user)):
    """Hent profilen til innlogget bruker."""
    return UserProfile(
        id=user.id,
        email=user.email,
        name=user.name,
        created_at=str(user.created_at),
    )
